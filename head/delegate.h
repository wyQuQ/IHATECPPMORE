#pragma once
#include <functional>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <atomic>

/// <summary>
/// 线程安全的多播委托（观察者）模板类。
///
/// 核心功能：
/// - 存储任意可调用对象（通过 `std::function<void(Args...)>`）。
/// - 通过 `add` 添加处理器并返回一个用于移除的 `Token`（类型为 `std::size_t`）。
/// - 通过 `remove(Token)` 根据 token 移除处理器，返回是否成功。
/// - 通过 `invoke(...)` 或重载的 `operator()` 广播调用所有已注册处理器。调用时会先复制当前处理器列表并释放锁，避免在回调中持锁执行用户代码。  
/// - 提供 `clear()` 清空所有处理器。  
/// - 使用内部互斥锁保证 `add/remove/clear/invoke` 的线程安全；`next_id_` 使用原子操作生成唯一 token。
///
/// 注意事项：
/// - 在回调执行期间不会持有内部 mutex，因此回调中可以安全地调用 `add` 或 `remove`（但对当前正在执行的调用不会影响那次已复制的副本）。  
/// - `Token` 从 1 开始自增；当达到 `std::size_t` 上限时会回绕（极端情况），请根据需求决定是否需要更复杂的回收策略。
///
/// 使用示例：
/// ```cpp
/// // 无参数无返回值
/// Delegate<> on_update;
/// auto t1 = on_update.add([](){ std::cout << "Subscriber A\n"; });
/// auto t2 = on_update.add([](){ std::cout << "Subscriber B\n"; });
/// on_update(); // 广播调用
/// on_update.remove(t1); // 移除订阅者 A
/// on_update.clear(); // 清空所有订阅者
/// ```
/// 带参数示例：
/// ```cpp
/// Delegate<int, const std::string&> on_event;
/// on_event.add([](int code, const std::string &msg){
///     std::cout << "code=" << code << " msg=" << msg << "\n";
/// });
/// on_event.invoke(42, "hello");
/// // 或者直接：
/// on_event(42, "hello");
/// ```
/// </summary>
/// <typeparam name="...Args">处理器接受的参数列表类型</typeparam>
template<typename... Args>
class Delegate {
public:
    using Handler = std::function<void(Args...)>;
    using Token = std::size_t;

    Delegate() : next_id_(0) {}

    // 添加处理器，返回用于移除的 token
    Token add(Handler h) {
        std::lock_guard<std::mutex> lock(mutex_);
        Token id = ++next_id_;
        handlers_.emplace(id, std::move(h));
        return id;
    }

    // 移除处理器，返回是否成功
    bool remove(Token id) {
        std::lock_guard<std::mutex> lock(mutex_);
        return handlers_.erase(id) > 0;
    }

    // 调用所有已注册处理器（不会在持锁时调用外部代码）
    void invoke(Args... args) const {
        std::vector<Handler> copy;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            copy.reserve(handlers_.size());
            for (const auto& p : handlers_) copy.push_back(p.second);
        }
        for (const auto& h : copy) {
            if (h) h(args...);
        }
    }

    // 便捷调用语法
    void operator()(Args... args) const { invoke(std::forward<Args>(args)...); }

    // 清空所有处理器
    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        handlers_.clear();
    }

private:
    mutable std::mutex mutex_;
    std::unordered_map<Token, Handler> handlers_;
    std::atomic<Token> next_id_;
};

// 多播委托：无参数、无返回值
extern Delegate<> main_thread_on_update;