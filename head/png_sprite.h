#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <atomic>
#include "cute_png_cache.h" // 使用 CF_Png / png_cache API

// 内部使用的帧表示，按 RGBA 字节序存储像素数据
struct PngFrame {
    std::vector<uint8_t> pixels; // r,g,b,a, r,g,b,a, ...
    int w = 0;
    int h = 0;

    bool empty() const noexcept { return pixels.empty() || w <= 0 || h <= 0; }
};

// 负责延迟加载 PNG 精灵资源并按帧提取的辅助类。
// 支持将一张竖直排列的图像分割为多帧（frameCount），并按全局帧计数与 frameDelay 获取当前帧。
class PngSprite {
public:
    PngSprite(const std::string& path = "", int frameCount = 1, int frameDelay = 1) noexcept;
    ~PngSprite();

    // 尝试加载 PNG 资源（延迟加载）。若已加载则立即返回 true。
    bool Load();
    // 卸载并释放资源，保证 noexcept
    void Unload() noexcept;

    const std::string& Path() const noexcept;
    int FrameCount() const noexcept;
    int FrameDelay() const noexcept;
    void SetFrameDelay(int delay) noexcept;

    // 修改路径（会在路径变化时卸载已有资源）
    void SetPath(const std::string& path) noexcept;
    // 清除路径并卸载资源
    void ClearPath() noexcept;
    // 是否设置了路径
    bool HasPath(std::string* out_path = nullptr) const noexcept;

    // 从图像中提取指定索引的帧（如果未加载会尝试延迟加载）
    PngFrame ExtractFrame(int index) const;
    // 使用内部的 m_frameCount 和全局帧计数返回当前帧
    PngFrame GetCurrentFrame() const;

    // 支持按自定义总帧数获取当前帧（例如竖直排列的帧数）
    PngFrame GetCurrentFrameWithTotal(int totalFrames) const;
    // 按自定义总帧数提取指定帧索引
    PngFrame ExtractFrameWithTotal(int index, int totalFrames) const;

    // 旋转
    float GetRotation() noexcept { return m_rotation; }
    void SetRotation(float rot) noexcept { m_rotation = rot; }
    void Rotate(float drot) noexcept { m_rotation += drot; }

    // 翻转
    bool m_flip_x = false;  // 水平翻转
    bool m_flip_y = false;  // 垂直翻转

	// 枢轴点
	CF_V2 get_pivot() const noexcept { return pivot; }
    void set_pivot(CF_V2 p) noexcept { pivot = p; }

private:
    // 从 CF_Png 图像中提取单帧像素（src 为整张图，totalFrames 表示竖直分割的帧数）
    PngFrame ExtractFrameFromImage(const CF_Png& src, int index, int totalFrames) const;

    std::string m_path;
    int m_frameCount = 1; // 原始帧数，默认为 1
    int m_frameDelay = 1; // 帧切换延迟，单位为帧（frame）
    CF_Png m_image{};     // 使用 Cute Framework 的 CF_Png 结构保存像素、宽高等信息
    bool m_loaded = false;

	float m_rotation = 0.0f; // 旋转角度（弧度制，1r = 2pi）

	CF_V2 pivot{ 0.0f, 0.0f }; // 旋转/缩放枢轴点（(0,0)表示贴图中央）
};

extern std::atomic<int> g_frame_count; // 使用全局帧计数