#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <cstdint>

#include <cute.h> // CF_Canvas

struct PngFrame;
class BaseObject;

// DrawingSequence 管理对象的帧上传与屏幕绘制流程，面向使用者的行为说明：
// - 提供对象的注册/注销接口以纳入统一的绘制序列。
// - 每帧分为两阶段：DrawAll（上传每个对象的像素到 per-owner canvas，"上传阶段"）
//   与 BlitAll（将已上传的 canvas 绘制到屏幕，"渲染阶段"）。
// - 线程安全：内部使用互斥保护注册表和回调读取，保证并发场景下状态一致性。
// - 保证渲染顺序的确定性（按 depth 与注册次序排序）。
class DrawingSequence {
public:
    using DrawCallback = std::function<void(const CF_Canvas&, const BaseObject*, int /*w*/, int /*h*/)>;

    static DrawingSequence& Instance() noexcept;

    // 将对象加入统一绘制序列。对象应在析构/不再需要时调用 Unregister。
    void Register(BaseObject* obj) noexcept;
    // 从绘制序列移除对象，同时释放相关缓存（如果存在）。
    void Unregister(BaseObject* obj) noexcept;

    // 上传阶段（Upload-only）：
    // - 在主循环的早期调用，负责逐对象提取当前帧像素并上传到 per-owner canvas 的目标纹理。
    // - 不执行最终绘制到屏幕的工作，保证纹理在渲染阶段之前已就绪。
    void DrawAll();

    // 渲染阶段（BlitAll）：
    // - 将之前上传好的 per-owner canvas 绘制到屏幕。
    // - 在绘制时会读取 BaseObject 的变换（位置、旋转、翻转、枢轴）并按约定执行变换。
    // - 内部对共享状态读取使用互斥以避免并发修改注册表/回调导致竞态。
    void BlitAll();

private:
    struct Entry {
        BaseObject* owner = nullptr;
        uint64_t reg_index = 0;
    };

    std::vector<std::unique_ptr<Entry>> m_entries;
    DrawCallback m_callback;
    mutable std::mutex m_mutex;

    uint64_t m_next_reg_index = 1;
};