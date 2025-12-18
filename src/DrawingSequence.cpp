#include "drawing_sequence.h"
#include "base_object.h"
#include "debug_config.h"
#include "UI_draw.h"
#include <algorithm>
#include <iostream>
#include <internal/cute_draw_internal.h>
#include <cstddef>
#include <unordered_set>
#include <vector>

extern std::atomic<int> g_frame_count;

static uint64_t last_image_id = CF_PREMADE_ID_RANGE_LO - 1;

// 每个批次最多积累的 sprite 数量，超过会立即打包执行一次 Flush
static constexpr size_t kSpriteChunkSize = 256;
// 作为临时缓存的 spriteentry 队列，避免直接向 s_draw 发送大量扩容请求
static std::vector<spritebatch_sprite_t> s_pending_sprites;

// 将当前缓存中的 sprite 条目打包到一个新的 CF_Command 并推送到 s_draw
static void FlushPendingSprites()
{
    if (!s_draw) return;
    if (s_pending_sprites.empty()) return;
    // 构造一个新命令并设置命令状态
    CF_Command& cmd = s_draw->add_cmd();
    for (const auto& entry : s_pending_sprites) {
        // 将所有缓冲条目写入命令的 items 数组
        cmd.items.add(entry);
    }
    // 清空缓存，待下一批填充
    s_pending_sprites.clear();
}

// 从 CF_Sprite 构造 spritebatch 条目并放到缓存中，必要时立即触发 Flush
static void PushFrameSprite(const CF_Sprite* spr, int frame_index, int frame_count)
{
    CF_Sprite sprite = *spr;
    if (!sprite.easy_sprite_id) return;

    spritebatch_sprite_t entry = {};
    entry.image_id = sprite.easy_sprite_id;
    entry.texture_id = 0;
    entry.sort_bits = 0;
    entry.w = sprite.w;
    entry.h = sprite.h;

    float frame_height_px = static_cast<float>(sprite.h);
    if (frame_count <= 1 || sprite.h <= 0) {
        entry.miny = 0.0f;
        entry.maxy = 1.0f;
    }
    else {
        constexpr float border_pixels = 1.0f;
        float usable_height_px = std::max(0.0f, static_cast<float>(sprite.h) - border_pixels * 2.0f);
        frame_height_px = usable_height_px / static_cast<float>(frame_count);
        float frame_step = frame_height_px / static_cast<float>(sprite.h);
        float border_uv = border_pixels / static_cast<float>(sprite.h);
        float epsilon = std::min(border_uv, 1.0f / static_cast<float>(sprite.h));
        entry.miny = border_uv + frame_step * frame_index;
        entry.maxy = std::min(1.0f - border_uv, entry.miny + frame_step - epsilon);
    }
    constexpr float horizontal_border_pixels = 1.0f;
    if (sprite.w > 0.0f) {
        float horizontal_border_uv = horizontal_border_pixels / static_cast<float>(sprite.w);
        entry.minx = horizontal_border_uv;
        entry.maxx = 1.0f - horizontal_border_uv;
    }
    else {
        entry.minx = 0.0f;
        entry.maxx = 1.0f;
    }

    CF_V2 pivot = -sprite.offset + (sprite.pivots ? sprite.pivots[sprite.frame_index] : CF_V2{ 0, 0 });
    CF_V2 pivot_scaled = cf_mul(pivot, sprite.scale);
    CF_V2 p = sprite.transform.p;
    CF_V2 scale = V2(sprite.scale.x * sprite.w, sprite.scale.y * frame_height_px);
    CF_V2 quad[4] = {
        {-0.5f,  0.5f},
        { 0.5f,  0.5f},
        { 0.5f, -0.5f},
        {-0.5f, -0.5f},
    };
    for (int i = 0; i < 4; ++i) {
        CF_V2 vertex = V2(quad[i].x * scale.x, quad[i].y * scale.y);
        CF_V2 relative = cf_sub(vertex, pivot_scaled);
        float x0 = sprite.transform.r.c * relative.x - sprite.transform.r.s * relative.y;
        float y0 = sprite.transform.r.s * relative.x + sprite.transform.r.c * relative.y;
        quad[i] = V2(x0 + p.x, y0 + p.y);
    }

    CF_M3x2 m = s_draw->mvp;
    entry.geom.shape[0] = cf_mul(m, quad[0]);
    entry.geom.shape[1] = cf_mul(m, quad[1]);
    entry.geom.shape[2] = cf_mul(m, quad[2]);
    entry.geom.shape[3] = cf_mul(m, quad[3]);
    entry.geom.type = BATCH_GEOMETRY_TYPE_SPRITE;
    entry.geom.is_sprite = true;
    entry.geom.color = cf_pixel_premultiply(cf_pixel_white());
    entry.geom.alpha = sprite.opacity;
    entry.geom.user_params = s_draw->user_params.last();
    entry.geom.fill = false;

    // 推入临时缓存，避免立即修改 s_draw 的命令列表
    s_pending_sprites.push_back(entry);
    if (s_pending_sprites.size() >= kSpriteChunkSize) {
        // 缓冲满时强制 flush，保持 Cute::Array 大小稳定
        FlushPendingSprites();
        s_pending_sprites.reserve(kSpriteChunkSize);
    }
}

DrawingSequence& DrawingSequence::Instance() noexcept
{
    static DrawingSequence instance;
    return instance;
}

void DrawingSequence::Register(BaseObject* obj) noexcept
{
    if (!obj) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    for (size_t i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i]->owner == obj) {
            OUTPUT(Header{ "DrawingSequence" },
                "Register skipped (already registered)", "obj=", obj,
                "table_index=", i, "reg_index=", m_entries[i]->reg_index);
            return;
        }
    }
    auto new_entry = std::make_unique<Entry>();
    new_entry->owner = obj;
    new_entry->reg_index = m_next_reg_index++;
    size_t table_index = m_entries.size();
    m_entries.push_back(std::move(new_entry));
    OUTPUT(Header{ "DrawingSequence" },
        "Registered obj=", obj,
        "table_index=", table_index,
        "reg_index=", m_entries.back()->reg_index);
}

void DrawingSequence::Unregister(BaseObject* obj) noexcept
{
    if (!obj) return;
    std::lock_guard<std::mutex> lock(m_mutex);
    auto it = std::find_if(m_entries.begin(), m_entries.end(),
        [obj](const std::unique_ptr<Entry>& entry) {
            return entry->owner == obj;
        });
    if (it == m_entries.end()) {
        OUTPUT(Header{ "DrawingSequence" },
            "Unregister failed (not found)", "obj=", obj);
        return;
    }
    size_t table_index = std::distance(m_entries.begin(), it);
    int reg_index = (*it)->reg_index;
    m_entries.erase(it);
    OUTPUT(Header{ "DrawingSequence" },
        "Unregistered obj=", obj,
        "table_index=", table_index,
        "reg_index=", reg_index);
}

void DrawingSequence::DrawAll()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    last_image_id = CF_PREMADE_ID_RANGE_LO - 1;
    s_pending_sprites.clear();
    s_pending_sprites.reserve(kSpriteChunkSize);
    // 按深度和注册顺序排序，保证渲染的稳定性
    std::sort(m_entries.begin(), m_entries.end(), [](const auto& a, const auto& b) {
        int depth_a = a->owner->GetDepth();
        int depth_b = b->owner->GetDepth();
        if (depth_a != depth_b) {
            return depth_a < depth_b;
        }
        return a->reg_index < b->reg_index;
    });

    for (const auto& entry : m_entries) {
        if (entry->owner && entry->owner->IsVisible()) {
            BaseObject* obj = entry->owner;
            CF_Sprite& sprite = obj->GetSprite();

            // 刷新动画
            cf_sprite_update(&sprite);

            // 使用对象位置更新 transform
            CF_V2 pos = obj->GetPosition();
            sprite.transform.p = pos;

            DrawUI::on_draw_ui.add(
                [=]() {obj->ShapeDraw(); }
            );
            if (!obj->m_collide_manifolds.empty()) {
                for (const CF_Manifold& m : obj->m_collide_manifolds)
                    DrawUI::on_draw_ui.add(
                        [=]() {obj->ManifoldDraw(m); }
                    );
            }

            if (obj->m_sprite_update_freq > 0 &&
                g_frame_count - obj->m_sprite_last_update_frame >= obj->m_sprite_update_freq)
            {
                obj->m_sprite_last_update_frame = g_frame_count;
                obj->m_sprite_current_frame_index = (obj->m_sprite_current_frame_index + 1) % obj->m_sprite_vertical_frame_count;
            }
            // 推送 sprite 条目到缓存
            PushFrameSprite(&sprite, obj->m_sprite_current_frame_index, obj->m_sprite_vertical_frame_count);
        }
    }

    // 帧末确保剩余条目被提交
    FlushPendingSprites();
}

size_t DrawingSequence::GetEstimatedMemoryUsageBytes() const noexcept
{
    size_t total = 0;
    total += m_entries.capacity() * sizeof(std::unique_ptr<Entry>);
    total += m_entries.size() * sizeof(Entry);
    return total;
}
