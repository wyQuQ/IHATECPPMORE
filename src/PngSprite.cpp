#include "png_sprite.h"

#include <cstring>
#include <algorithm>
#include <iostream>   // 错误与诊断输出
#include <string>

#include "debug_config.h"
#include "cute_png_cache.h"      // CF_Png + png_cache API
#include "cute_file_system.h"    // CF_Path + fs_get_backend_specific_error_message
#include "cute_result.h" // 辅助的 CF_Result / 错误检查

// PNG 加载调试日志开关（由 debug_config.h 中的 PNG_LOAD_DEBUG 控制）
#if PNG_LOAD_DEBUG
#define PNG_LOAD_LOG(x) do { std::cerr << x; } while(0)
#else
#define PNG_LOAD_LOG(x) do {} while(0)
#endif

extern std::atomic<int> g_frame_count; // 全局帧计数（用于动画时间）

// 构造：仅记录路径和帧配置，不做耗时 IO 操作（懒加载设计）
PngSprite::PngSprite(const std::string& path, int frameCount, int frameDelay) noexcept
    : m_path(path)
    , m_frameCount(frameCount > 0 ? frameCount : 1)
    , m_frameDelay(frameDelay > 0 ? frameDelay : 1)
    , m_image{ 0 }
    , m_loaded(false)
{
    // 资源加载采用懒惰策略：构造时不执行磁盘 IO
}

PngSprite::~PngSprite()
{
    // 析构时确保资源已被释放
    Unload();
}

// Load：尝试使用多种路径策略加载 PNG 资源，使用 png_cache 做缓存管理。
// 成功时设置 m_loaded 并保留 CF_Png 句柄供后续帧提取使用。
bool PngSprite::Load()
{
    if (m_loaded) return true;

    PNG_LOAD_LOG("[PngSprite] Attempting to load PNG: '" << m_path << "'\n");

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

    // 失败时尝试以下回退路径策略以提升资源加载的容错性：
    // 1) 去掉开头的 '/' 之后重试
    if (!m_path.empty() && m_path.front() == '/') {
        std::string p = m_path.substr(1);
        PNG_LOAD_LOG("[PngSprite] Primary load failed, trying without leading slash: '" << p << "'\n");
        CF_Png tmp = cf_png_defaults();
        res = Cute::png_cache_load(p.c_str(), &tmp);
        if (!cf_is_error(res) && tmp.pix) {
            m_image = tmp;
            if (m_frameCount <= 0) m_frameCount = 1;
            m_loaded = true;
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

    // 2) 以程序基目录拼接绝对路径重试
    Cute::CF_Path base = Cute::fs_get_base_directory();
    base.normalize();
    std::string base_s = base.c_str();
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

    // 3) 尝试 base/content/... 路径以兼容轻量资源组织约定
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

    // 所有策略均失败则返回 false
    PNG_LOAD_LOG("[PngSprite] Failed to load PNG. Tried paths:\n"
        << "  1) " << m_path << "\n"
        << "  2) " << (m_path.size() && m_path.front() == '/' ? m_path.substr(1) : std::string("(n/a)")) << "\n"
        << "  3) " << full << "\n"
        << "  4) " << full_content << "\n");
    return false;
}

// Unload：释放已加载的 png_cache 资源，保证可以多次 Load/Unload
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

// SetPath：设置资源路径并在路径改变时卸载旧资源以便下次访问重载新资源
void PngSprite::SetPath(const std::string& path) noexcept
{
    if (m_path == path) return;
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

// 提取指定帧：若尚未加载会触发懒加载
PngFrame PngSprite::ExtractFrame(int index) const
{
    if (!m_loaded)
    {
        // 懒加载以避免在构造期间进行 IO
        const_cast<PngSprite*>(this)->Load();
    }

    if (!m_image.pix || m_frameCount <= 0) return {};

    int idx = index % m_frameCount;
    if (idx < 0) idx += m_frameCount;
    return ExtractFrameFromImage(m_image, idx, m_frameCount);
}

// 获取当前帧（基于全局帧计数），适用于常规动画播放
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

// 支持自定义总帧数的当前帧获取（适用于多行/分片图集）
PngFrame PngSprite::GetCurrentFrameWithTotal(int totalFrames) const
{
    if (!m_loaded) const_cast<PngSprite*>(this)->Load();
    if (!m_image.pix || totalFrames <= 0) return {};

    int global = static_cast<int>(g_frame_count.load());
    int idx = (global / std::max(1, m_frameDelay)) % totalFrames;
    if (idx < 0) idx += totalFrames;
    return ExtractFrameFromImage(m_image, idx, totalFrames);
}

PngFrame PngSprite::ExtractFrameWithTotal(int index, int totalFrames) const
{
    if (!m_loaded) const_cast<PngSprite*>(this)->Load();
    if (!m_image.pix || totalFrames <= 0) return {};

    int idx = index % totalFrames;
    if (idx < 0) idx += totalFrames;
    return ExtractFrameFromImage(m_image, idx, totalFrames);
}

// 从 CF_Png 图像中按垂直帧数拆分并复制出单帧像素数据（返回 PngFrame）
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