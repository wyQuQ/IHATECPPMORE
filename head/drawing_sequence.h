#pragma once

#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <cstdint>

#include <cute.h> // CF_Canvas

struct PngFrame;
class BaseObject;

class DrawingSequence {
public:
    using DrawCallback = std::function<void(const CF_Canvas&, const BaseObject*, int /*w*/, int /*h*/)>;

    static DrawingSequence& Instance() noexcept;

    void Register(BaseObject* obj) noexcept;
    void Unregister(BaseObject* obj) noexcept;

    void DrawAll();

private:
    struct Entry {
        BaseObject* owner = nullptr;
        uint64_t reg_index = 0;
    };

    std::vector<std::unique_ptr<Entry>> m_entries;
    mutable std::mutex m_mutex;

    uint64_t m_next_reg_index = 1;
};