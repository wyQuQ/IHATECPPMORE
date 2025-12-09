# DrawingSequence

## 概述  
`DrawingSequence` 负责管理需要渲染的对象集合并提供两阶段渲染流水线：上传阶段（`DrawAll()`）与呈现阶段（`BlitAll()`）。设计目标是保证每帧像素数据上传（texture 更新）与最终绘制的顺序与一致性，降低 GPU 同步开销并支持 per-owner canvas 缓存。

## 核心职责
- 管理注册（`Register(BaseObject*)`）与注销（`Unregister(BaseObject*)`）的对象集合。
- 在 `DrawAll()` 中遍历注册对象，基于对象当前状态（visible/depth/frame/pivot/rotation/scale）提取像素并上传到各自的 per-owner `CF_Canvas`（或纹理缓存）。
- 在 `BlitAll()` 中按深度与注册顺序绘制已上传的 canvas 到最终目标。

## 两阶段流程要点
- DrawAll（上传阶段）
  - 在主循环中应在逻辑更新之后调用（通常在 `ObjManager::UpdateAll()` 后），以保证上传的像素反映刚刚完成的逻辑变更（例如新贴图、frame 变化、pivot 修改）。
  - 每个对象通过 `SpriteGetFrame()` 获取当前帧的 `PngFrame`，并更新其对应的 canvas/texture。
  - 更新应尽量批量化或使用缓存以减少 GPU 纹理重建与内存分配。
- BlitAll（呈现阶段）
  - 在渲染线程/渲染阶段调用，读取已上传的 texture 并按顺序绘制到屏幕。
  - 根据 `GetDepth()` 和注册顺序决定绘制顺序，确保层级关系可控。

## 并发与错误处理
- DrawingSequence 可能维护互斥体（例如 `m_mutex` 或 `g_canvas_cache_mutex`）来保护 canvas 缓存与注册集合，避免多线程竞态。调用方应在跨线程访问时遵循约定。
- 上传/删除等操作应尽量在主线程完成；若异步上传需保证引用生命周期与线程同步。
- 在上传或 blit 时应捕获并记录异常/错误（如果适用），保证渲染阶段不会使主循环崩溃。

## 实现注意
- 使用 per-owner canvas 缓存（例如按 `BaseObject*` 缓存 `CF_Canvas`）能显著减少重复分配与纹理切换。
- 深度排序：在 `BlitAll` 中应使用 `GetDepth()` 作为主要排序键，注册顺序作为次级键以保证可预测的渲染顺序.