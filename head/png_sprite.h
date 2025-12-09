#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include "cute_png_cache.h" // 使用 CF_Png / png_cache API
#include "v2math.h"

// PngFrame 表示单帧的 RGBA 像素数据和尺寸信息，供渲染或上传使用。
// - pixels 按 r,g,b,a 顺序平铺（宽度 * 高度 * 4 bytes）
// - w/h 为像素尺寸，若 empty() 返回 true 则表示帧不可用
struct PngFrame {
    std::vector<uint8_t> pixels; // r,g,b,a, r,g,b,a, ...
    int w = 0;
    int h = 0;

    bool empty() const noexcept { return pixels.empty() || w <= 0 || h <= 0; }
};

// PngSprite 提供面向使用者的 PNG 精灵资源管理：
// - 加载/卸载 PNG 资源（支持帧拆分与循环播放控制）
// - 提取单帧或基于全局帧计数的当前帧（便于统一的动画时序）
// - 支持设置帧率、路径与旋转/翻转/枢轴（pivot）等属性
// 使用说明：构造后调用 Load() 以实际加载资源；GetCurrentFrame() 在必要时会自动触发懒加载。
class PngSprite {
public:
    PngSprite(const std::string& path = "", int frameCount = 1, int frameDelay = 1) noexcept;
    ~PngSprite();

    // 载入资源。若已载入则返回 true；失败返回 false。
    // - 调用者可在 Start() 中或渲染线程中调用 Load()（取决于资源管理策略）
    bool Load();
    // 卸载并释放资源，保证 noexcept
    void Unload() noexcept;

    const std::string& Path() const noexcept;
    int FrameCount() const noexcept;
    int FrameDelay() const noexcept;
    void SetFrameDelay(int delay) noexcept;

    // 修改资源路径时会自动卸载旧资源
    void SetPath(const std::string& path) noexcept;
    // 清除路径并卸载资源
    void ClearPath() noexcept;
    // 查询是否存在路径（可选输出当前路径）
    bool HasPath(std::string* out_path = nullptr) const noexcept;

    // 提取指定索引的帧（会在需要时触发懒加载）
    // - index 范围应在 [0, FrameCount()-1] 内
    PngFrame ExtractFrame(int index) const;
    // 使用内部 frameCount 返回当前帧（受全局 g_frame_count 控制）
    // - 该方法基于 g_frame_count 和 m_frameDelay 计算当前帧索引以实现全局同步动画
    PngFrame GetCurrentFrame() const;

    // 支持外部传入总帧数以获取当前帧（用于垂直排列多帧图集）
    PngFrame GetCurrentFrameWithTotal(int totalFrames) const;
    // 在给定 totalFrames 的情况下提取指定帧
    PngFrame ExtractFrameWithTotal(int index, int totalFrames) const;

    // 旋转属性接口（以弧度为单位）
    // - 旋转值规范化到 [-pi, pi] 以减少累积误差
    float GetSpriteRotation() const noexcept { return m_rotation; }
    void SetSpriteRotation(float rot) noexcept { 
        if (rot > pi) rot -= 2 * pi;
        else if (rot < -pi) rot += 2 * pi;
        m_rotation = rot; 
    }
    void RotateSprite(float drot) noexcept { 
        m_rotation += drot; 
        if (m_rotation > pi) m_rotation -= 2 * pi;
        else if (m_rotation < -pi) m_rotation += 2 * pi;
    }

    // 翻转标志（供渲染使用）
    bool m_flip_x = false;  // 水平翻转
    bool m_flip_y = false;  // 垂直翻转

	// 缩放属性接口（仅存储，实际渲染/碰撞转换由调用者使用）
	float get_scale_x() const noexcept { return m_scale_x; }
	void set_scale_x(float sx) noexcept { m_scale_x = sx; }
	float get_scale_y() const noexcept { return m_scale_y; }
	void set_scale_y(float sy) noexcept { m_scale_y = sy; }

    // 枢轴（pivot）用于旋转/对齐，单位为像素，相对于贴图中心
    CF_V2 get_pivot() const noexcept { return pivot; }
    void set_pivot(CF_V2 p) noexcept { pivot = p; }

private:
    // 从单张 CF_Png 图像中按 totalFrames 拆分并提取指定的帧像素
    // - 实现负责根据 totalFrames 切分垂直排列图集并返回像素缓冲（复制）
    PngFrame ExtractFrameFromImage(const CF_Png& src, int index, int totalFrames) const;

    std::string m_path;
    int m_frameCount = 1; // 垂直帧数（默认 1）
    int m_frameDelay = 1; // 帧延迟（用于动画速度控制）
    CF_Png m_image{};     // Cute Framework 的 CF_Png 资源句柄
    bool m_loaded = false;

    float m_rotation = 0.0f; // 旋转角度（弧度）
	float m_scale_x = 1.0f; // 水平缩放
	float m_scale_y = 1.0f; // 垂直缩放

    CF_V2 pivot{ 0.0f, 0.0f }; // 旋转/翻转枢轴点（以像素为单位）
};

extern std::atomic<int> g_frame_count; // 全局帧计数，用于同步动画时间