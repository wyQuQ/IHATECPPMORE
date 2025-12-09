# PngSprite

## 说明
- `PngSprite` 提供 PNG 图像资源的加载/缓存/帧拆分与动画时序支持，封装了对 Cute Framework png_cache 的调用。
- 特点：懒加载、支持竖直排列的多帧图集、多帧延迟（frameDelay）与全局帧计数同步（`g_frame_count`）。

## 主要职责
- 加载与卸载 PNG（`Load()` / `Unload()`，`Load()` 支持多种回退路径）。
- 提取单帧像素数据（`ExtractFrame`、`ExtractFrameFromImage`）。
- 给上层提供当前帧的便捷接口（`GetCurrentFrame()`、`GetCurrentFrameWithTotal(totalFrames)`）。
- 提供旋转/翻转/缩放/枢轴状态以便渲染子系统使用。

## 关键接口
- 资源管理：
  - `bool Load()`：尝试加载，成功返回 true（可能修改 `m_path` 为实际匹配路径）。
  - `void Unload()`：释放加载资源（noexcept）。
  - `void SetPath(const std::string&)` / `void ClearPath()` / `bool HasPath(std::string* out_path)`。

- 帧与动画：
  - `PngFrame ExtractFrame(int index)`：按 `m_frameCount` 拆分并返回指定帧像素。
  - `PngFrame GetCurrentFrame()`：基于全局 `g_frame_count` 与 `m_frameDelay` 返回当前帧（懒加载）。
  - `void SetFrameDelay(int delay)`：设置动画速度（以游戏帧为单位）。

- 变换属性：
  - `float GetSpriteRotation()` / `SetSpriteRotation()` / `RotateSprite()`。
  - `bool m_flip_x/m_flip_y`。
  - `float get_scale_x()/set_scale_x()`、`get_scale_y()/set_scale_y()`。
  - `CF_V2 get_pivot()/set_pivot()`：枢轴，单位像素，相对于贴图中心。

## 实现要点
- `Load()` 首先尝试直接路径，若失败会尝试去掉 leading '/'、拼接 base 目录、或 base+"/content" 等策略以提高容错率。
- 数据组织：`CF_Png m_image` 持有像素，`ExtractFrameFromImage` 通过复制行数据返回 `PngFrame`（包含宽高与 RGBA 字节缓存）。
- `GetCurrentFrame()` 在未加载时会触发懒加载（`Load()`）。