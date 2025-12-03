#include "png_sprite.h"

#include <cstring>
#include <algorithm>
#include <iostream>   // 新增：用于调试输出
#include <string>

#include "debug_config.h"
#include "cute_png_cache.h"      // CF_Png + png_cache API
#include "cute_file_system.h"    // CF_Path + fs_get_backend_specific_error_message
#include "cute_result.h" // 如需 cf_is_error / cf_result helpers

// 根据 debug_config.h 中的 PNG_LOAD_DEBUG 控制加载时的调试输出
#if PNG_LOAD_DEBUG
#define PNG_LOAD_LOG(x) do { std::cerr << x; } while(0)
#else
#define PNG_LOAD_LOG(x) do {} while(0)
#endif

extern std::atomic<int> g_frame_count; // 使用全局帧计数

PngSprite::PngSprite(const std::string& path, int frameCount, int frameDelay) noexcept
    : m_path(path)
    , m_frameCount(frameCount > 0 ? frameCount : 1)
    , m_frameDelay(frameDelay > 0 ? frameDelay : 1)
    , m_image{ 0 }
    , m_loaded(false)
{
    // 这里保持延迟加载（不要在构造中进行磁盘 IO）
}

PngSprite::~PngSprite()
{
    // 确保析构时清理资源；Unload() 已标注 noexcept
    Unload();
}

bool PngSprite::Load()
{
    if (m_loaded) return true;

    // 诊断日志：记录尝试加载的原始路径 
    PNG_LOAD_LOG("[PngSprite] Attempting to load PNG: '" << m_path << "'\n");

    // 首次尝试：直接用当前路径
    CF_Png img = cf_png_defaults();
    CF_Result res = Cute::png_cache_load(m_path.c_str(), &img);
    if (!cf_is_error(res) && img.pix) {
        m_image = img;
        if (m_frameCount <= 0) m_frameCount = 1;
        m_loaded = true;
        PNG_LOAD_LOG("[PngSprite] Loaded using path: '" << m_path << "'\n");
        return true;
    } else {
        const char* backend_err = cf_fs_get_backend_specific_error_message();
        if (cf_is_error(res)) {
            PNG_LOAD_LOG("[PngSprite] png_cache_load failed for '" << m_path << "' (CF_Result error). Backend: "
                      << (backend_err ? backend_err : "(none)") << "\n");
        } else if (!img.pix) {
            PNG_LOAD_LOG("[PngSprite] png_cache_load returned no pixels for '" << m_path << "'. Backend: "
                      << (backend_err ? backend_err : "(none)") << "\n");
        }
    }

    // 如果失败，尝试常见变体并记录
    // 1) 去掉前导 '/'（如果有）
    if (!m_path.empty() && m_path.front() == '/') {
        std::string p = m_path.substr(1);
        PNG_LOAD_LOG("[PngSprite] Primary load failed, trying without leading slash: '" << p << "'\n");
        CF_Png tmp = cf_png_defaults();
        res = Cute::png_cache_load(p.c_str(), &tmp);
        if (!cf_is_error(res) && tmp.pix) {
            m_image = tmp;
            if (m_frameCount <= 0) m_frameCount = 1;
            m_loaded = true;
            // 同步 m_path 为实际可用路径，便于后续调试
            m_path = p;
            PNG_LOAD_LOG("[PngSprite] Loaded using path: '" << m_path << "'\n");
            return true;
        } else {
            const char* backend_err = cf_fs_get_backend_specific_error_message();
            if (cf_is_error(res)) {
                PNG_LOAD_LOG("[PngSprite] png_cache_load failed for '" << p << "' (CF_Result error). Backend: "
                          << (backend_err ? backend_err : "(none)") << "\n");
            } else if (!tmp.pix) {
                PNG_LOAD_LOG("[PngSprite] png_cache_load returned no pixels for '" << p << "'. Backend: "
                          << (backend_err ? backend_err : "(none)") << "\n");
            }
        }
    }

    // 2) 使用可执行/基础目录拼接绝对路径（在未挂载或挂载失败的场景下有用）
    Cute::CF_Path base = Cute::fs_get_base_directory();
    base.normalize();
    std::string base_s = base.c_str();
    // 如果 m_path 已经以 '/' 开始，直接拼接 base + m_path，否则加 '/' 再拼
    std::string full = base_s + (m_path.empty() || m_path.front() != '/' ? std::string("/") + m_path : m_path);
    PNG_LOAD_LOG("[PngSprite] Trying absolute path: '" << full << "'\n");
    {
        CF_Png tmp = cf_png_defaults();
        res = Cute::png_cache_load(full.c_str(), &tmp);
        if (!cf_is_error(res) && tmp.pix) {
            m_image = tmp;
            if (m_frameCount <= 0) m_frameCount = 1;
            m_loaded = true;
            m_path = full;
            PNG_LOAD_LOG("[PngSprite] Loaded using absolute path: '" << m_path << "'\n");
            return true;
        } else {
            const char* backend_err = cf_fs_get_backend_specific_error_message();
            if (cf_is_error(res)) {
                PNG_LOAD_LOG("[PngSprite] png_cache_load failed for '" << full << "' (CF_Result error). Backend: "
                          << (backend_err ? backend_err : "(none)") << "\n");
            } else if (!tmp.pix) {
                PNG_LOAD_LOG("[PngSprite] png_cache_load returned no pixels for '" << full << "'. Backend: "
                          << (backend_err ? backend_err : "(none)") << "\n");
            }
        }
    }

    // 3) 如果项目中资源位于 base/content/...，也尝试 base + "/content" + m_path
    std::string full_content = base_s + "/content" + (m_path.empty() || m_path.front() != '/' ? std::string("/") + m_path : m_path);
    PNG_LOAD_LOG("[PngSprite] Trying base/content path: '" << full_content << "'\n");
    {
        CF_Png tmp = cf_png_defaults();
        res = Cute::png_cache_load(full_content.c_str(), &tmp);
        if (!cf_is_error(res) && tmp.pix) {
            m_image = tmp;
            if (m_frameCount <= 0) m_frameCount = 1;
            m_loaded = true;
            m_path = full_content;
            PNG_LOAD_LOG("[PngSprite] Loaded using base/content path: '" << m_path << "'\n");
            return true;
        } else {
            const char* backend_err = cf_fs_get_backend_specific_error_message();
            if (cf_is_error(res)) {
                PNG_LOAD_LOG("[PngSprite] png_cache_load failed for '" << full_content << "' (CF_Result error). Backend: "
                          << (backend_err ? backend_err : "(none)") << "\n");
            } else if (!tmp.pix) {
                PNG_LOAD_LOG("[PngSprite] png_cache_load returned no pixels for '" << full_content << "'. Backend: "
                          << (backend_err ? backend_err : "(none)") << "\n");
            }
        }
    }

    // 全部尝试失败：保持无载入状态并返回 false
    PNG_LOAD_LOG("[PngSprite] Failed to load PNG. Tried paths:\n"
        << "  1) " << m_path << "\n"
        << "  2) " << (m_path.size() && m_path.front() == '/' ? m_path.substr(1) : std::string("(n/a)")) << "\n"
        << "  3) " << full << "\n"
        << "  4) " << full_content << "\n");
    return false;
}

void PngSprite::Unload() noexcept
{
    if (m_loaded)
    {
        if (m_image.pix) Cute::png_cache_unload(m_image);
        m_image.pix = nullptr;
        m_image.w = m_image.h = 0;
        m_loaded = false;
    }
}

const std::string& PngSprite::Path() const noexcept { return m_path; }
int PngSprite::FrameCount() const noexcept { return m_frameCount; }
int PngSprite::FrameDelay() const noexcept { return m_frameDelay; }
void PngSprite::SetFrameDelay(int delay) noexcept { m_frameDelay = (delay > 0 ? delay : 1); }

// 新增实现
void PngSprite::SetPath(const std::string& path) noexcept
{
    if (m_path == path) return;
    // 路径变更，卸载已有资源以避免残留
    Unload();
    m_path = path;
}

void PngSprite::ClearPath() noexcept
{
    Unload();
    m_path.clear();
}

bool PngSprite::HasPath(std::string* out_path) const noexcept
{
	bool has = !m_path.empty();
    if (has && out_path != nullptr)*out_path = m_path;
    return has;
}

PngFrame PngSprite::ExtractFrame(int index) const
{
    if (!m_loaded)
    {
        // 尝试延迟加载（通常在主线程）
        const_cast<PngSprite*>(this)->Load();
    }

    if (!m_image.pix || m_frameCount <= 0) return {};

    int idx = index % m_frameCount;
    if (idx < 0) idx += m_frameCount;
    return ExtractFrameFromImage(m_image, idx, m_frameCount);
}

PngFrame PngSprite::GetCurrentFrame() const
{
    if (!m_loaded)
    {
        const_cast<PngSprite*>(this)->Load();
    }

    if (!m_image.pix || m_frameCount <= 0) return {};

    int global = static_cast<int>(g_frame_count.load());
    int idx = (global / std::max(1, m_frameDelay)) % m_frameCount;
    if (idx < 0) idx += m_frameCount;
    return ExtractFrameFromImage(m_image, idx, m_frameCount);
}

// 新增：按自定义总帧数（例如竖排帧数）返回当前帧
PngFrame PngSprite::GetCurrentFrameWithTotal(int totalFrames) const
{
    if (!m_loaded) const_cast<PngSprite*>(this)->Load();
    if (!m_image.pix || totalFrames <= 0) return {};

    int global = static_cast<int>(g_frame_count.load());
    int idx = (global / std::max(1, m_frameDelay)) % totalFrames;
    if (idx < 0) idx += totalFrames;
    return ExtractFrameFromImage(m_image, idx, totalFrames);
}

// 新增：按自定义总帧数提取指定帧索引
PngFrame PngSprite::ExtractFrameWithTotal(int index, int totalFrames) const
{
    if (!m_loaded) const_cast<PngSprite*>(this)->Load();
    if (!m_image.pix || totalFrames <= 0) return {};

    int idx = index % totalFrames;
    if (idx < 0) idx += totalFrames;
    return ExtractFrameFromImage(m_image, idx, totalFrames);
}

PngFrame PngSprite::ExtractFrameFromImage(const CF_Png& src, int index, int totalFrames) const
{
    PngFrame out;
    if (!src.pix || totalFrames <= 0) return out;

    int frame_h = src.h / totalFrames;
    if (frame_h <= 0) return out;

    int w = src.w;
    int h = frame_h;
    size_t row_bytes = static_cast<size_t>(w) * sizeof(CF_Pixel);
    size_t total_bytes = row_bytes * (size_t)h;

    out.pixels.resize(total_bytes);
    out.w = w;
    out.h = h;

    int start_row = index * frame_h;
    for (int y = 0; y < frame_h; ++y)
    {
        const uint8_t* src_row = reinterpret_cast<const uint8_t*>(src.pix + (size_t)(start_row + y) * (size_t)w);
        uint8_t* dst_row = out.pixels.data() + (size_t)y * row_bytes;
        std::memcpy(dst_row, src_row, row_bytes);
    }

    return out;
}