#pragma once
#include <vector>
#include <functional>
#include <mutex>
#include <memory>
#include "delegate.h"
#include "cute_coroutine.h"

class BaseObject; // 前向声明，避免头文件循环引用

/// <summary>
/// ActSeq - 一个轻量的动作链（帧为单位）执行器。
///
/// 设计目标（模仿 `delegate.h` 的风格与注释习惯）：
/// - 存储若干按顺序执行的动作（每个动作包含持续帧数和要执行的回调）。
/// - 当调用 `play` 时，为指定的 `BaseObject*` 创建并启动一个 Coroutine，按动作链顺序执行动作；每个动作会在指定帧数内每帧调用回调，并传递进度信息（当前帧/总帧数）。
/// - 该实现将协程的 resume 工作挂载到全局的 `main_thread_on_update`（类型见 `delegate.h`），每帧由该委托驱动一次 resume，协程结束后自动从委托移除并销毁协程及上下文内存。
/// - 线程安全：对动作链的修改受互斥锁保护（`add/clear`）。
///
/// 注意事项：
/// - 持续时间使用"帧数（frames）"单位；调用方应确保 `main_thread_on_update` 在每帧被触发一次（通常在主循环）。
/// - 动作回调签名为 `std::function<void(BaseObject*, int, int)>`，参数为：对象指针、当前帧索引（0-based）、总帧数。
/// - 协程资源：本类在 `play` 内部分配一个上下文指针（在协程结束时由 update-lambda 清理），并在协程死亡时调用 `Cute::destroy_coroutine` 来释放协程句柄。
/// </summary>
class ActSeq {
public:
    using Handler = std::function<void(BaseObject*, int, int)>; // 修改：传递当前帧和总帧数
    using Frames = int;

    ActSeq() = default;

    // 添加一个动作，返回该动作在链中的索引（从0开始）
    size_t add(Frames frames, Handler h) {
        std::lock_guard<std::mutex> lg(mutex_);
        steps_.push_back(Step{ frames, std::move(h) });
        return steps_.size() - 1;
    }

    // 清空动作链
    void clear() {
        std::lock_guard<std::mutex> lg(mutex_);
        steps_.clear();
    }

    // 是否为空
    bool empty() const {
        std::lock_guard<std::mutex> lg(mutex_);
        return steps_.empty();
    }

    // 播放动作链：为指定对象创建协程并立即开始（由 main_thread_on_update 驱动）
    // 返回 true 表示已成功创建协程并开始播放；返回 false 表示动作链为空或创建失败。
    bool play(BaseObject* obj, bool loop = false) {
        CF_Coroutine co;
        {
            std::lock_guard<std::mutex> lg(mutex_);
            if (steps_.empty() || obj == nullptr) return false;

            // 上下文结构，传入协程的 udata
            struct Context {
                ActSeq const* seq;
                BaseObject* obj;
                CF_Coroutine co;
                std::size_t update_token;
                bool loop;
            };

            // 分配上下文并填入序列指针与对象指针；co 待会设置
            Context* ctx = new Context{ this, obj, CF_Coroutine{0}, 0, loop };

            // 协程入口（C 风格函数指针），从 udata 获取 ctx 并按步执行
            auto coroutine_fn = [](CF_Coroutine co) {
                Context* c = static_cast<Context*>(Cute::coroutine_get_udata(co));
                if (!c || !c->seq) return;
                const ActSeq* seq = c->seq;

                // 逐步执行动作链：在指定帧数内每帧调用回调
                for (size_t i = 0; ; ++i) {
                    // 读取当前步骤线程安全地（只读访问，允许并发修改）
                    ActSeq::Step step;
                    {
                        std::lock_guard<std::mutex> lg(seq->mutex_);
                        if (i >= seq->steps_.size()) {
                            if (!c->loop) break; // ���û�г�ѭ���˳�
                            i = 0; // ��������������ʼ
                        }
                        step = seq->steps_[i % seq->steps_.size()];
                    }

                    // 修改：在指定帧数内每帧调用回调，并传递进度信息
                    for (int f = 0; f < step.frames; ++f) {
                        if (step.h) step.h(c->obj, f, step.frames);
                        Cute::coroutine_yield(co);
                    }
                }

                // 协程函数返回 -> 状态会变为 DEAD，交由 update-lambda 回收资源（见 play 中的 lambda）
            };

            // 创建协程（传入 ctx 作为 udata）
            co = Cute::make_coroutine(coroutine_fn, 0, ctx);
            ctx->co = co;

            // 将协程的 resume 挂载到全局帧更新委托，负责每帧 resume，并在协程结束时清理资源
            ctx->update_token = main_thread_on_update.add([ctx]() {
                // 如果协程还活着，则 resume；否则回收
                if (Cute::coroutine_state(ctx->co) != CF_COROUTINE_STATE_DEAD) {
                    Cute::coroutine_resume(ctx->co);
                } else {
                    // 移除自己并释放资源
                    main_thread_on_update.remove(ctx->update_token);
                    Cute::destroy_coroutine(ctx->co);
                    delete ctx;
                }
            });
        }

        // 开始第一次 resume（可不做，也可以在下一帧由委托驱动；这里立即开始一次以马上触发动作）
        // 移到锁作用域之外，避免在持有 mutex_ 时调用协程导致递归锁
        Cute::coroutine_resume(co);

        return true;
    }

private:
    struct Step {
        Frames frames;
        Handler h;
    };

    mutable std::mutex mutex_;
    std::vector<Step> steps_;
};