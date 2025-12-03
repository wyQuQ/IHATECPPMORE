#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <cstdint>

#include <cute.h> // CF_Canvas

struct PngFrame;
class BaseObject;

// 回调：只负责绘制 DrawingSequence 已上传好的 canvas（不负责上传）
class DrawingSequence {
public:
    using DrawCallback = std::function<void(const CF_Canvas&, const BaseObject*, int /*w*/, int /*h*/)>;

    static DrawingSequence& Instance() noexcept;

    void Register(BaseObject* obj) noexcept;
    void Unregister(BaseObject* obj) noexcept;

    // 每帧遍历：Upload-only（提取帧并上传到 per-owner canvas）
    void DrawAll();

    // 将已上传的 canvas 实际绘制到场景（由主循环在合适时刻调用）
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