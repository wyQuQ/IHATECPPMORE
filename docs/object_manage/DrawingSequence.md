# DrawingSequence

## 目录
- [概述](#概述)
- [类 `DrawingSequence`](#类-drawingsequence)
  - [类型别名 `DrawCallback`](#类型别名-drawcallback)
  - [全局 Canvas 缓存（实现细节）](#全局-canvas-缓存实现细节)
  - [属性 `m_entries` 与 `Entry`](#属性-mentries-与-entry)
  - [互斥与并发](#互斥与并发)
  - [方法 `Instance()`](#方法-instance)
  - [方法 `Register(BaseObject* obj)`](#方法-registerbaseobject-obj)
  - [方法 `Unregister(BaseObject* obj)`](#方法-unregisterbaseobject-obj)
  - [方法 `SetDrawCallback(DrawCallback cb)`](#方法-setdrawcallbackdrawcallback-cb)
  - [方法 `DrawAll()`（Upload-only）](#方法-drawallupload-only)
  - [方法 `BlitAll()`（实际绘制）](#方法-blitall实际绘制)
- [设计要点与边界情况（摘要）](#设计要点与边界情况摘要)

该文档详尽介绍 `DrawingSequence` 类的职责、内部结构与每个公开方法的参数与实现意图。

---

类 `DrawingSequence` {
- 类型别名 `DrawCallback` {
  - 作用：Blit 阶段的绘制回调签名，供外部设置以在最终渲染时把已上传的 per-owner canvas 绘制到场景或自定义目标。
  - 签名：`std::function<void(const CF_Canvas&, const BaseObject*, int /*w*/, int /*h*/)>`。
  - 约定：回调仅负责“绘制”已存在的 `CF_Canvas`（不负责上传或管理 canvas 的创建/销毁）。回调若抛异常会被 `DrawingSequence` 捕获并记录。
}

, 全局 Canvas 缓存（实现细节） {
  - 作用：为每个拥有贴图的 `BaseObject*` 保留一个 per-owner `CF_Canvas`，避免每帧重复创建/销毁 canvas。
  - 存储：实现文件中有命名空间静态对象：
    - `struct CanvasCache { CF_Canvas canvas; int w; int h; };`
    - `static std::unordered_map<BaseObject*, CanvasCache> g_canvas_cache;`
    - `static std::mutex g_canvas_cache_mutex;` —— 保护 `g_canvas_cache` 的并发访问。
  - 语义：
    - `DrawAll()` 负责根据帧像素数据为每个对象创建或重置缓存 canvas 并上传像素到对应 texture。
    - `Unregister()` 会在移除注册时销毁并从 `g_canvas_cache` 中移除对应条目以释放资源。
}

, 属性 `m_entries` 与 `Entry` {
  - 作用：保持注册对象列表（用于按注册顺序和 depth 排序后的遍历）。
  - 类型：`std::vector<std::unique_ptr<Entry>>`，其中 `Entry` 包含：
    - `BaseObject* owner`：裸指针，指向拥有该 per-owner canvas 的对象（生命周期由外部管理）。
    - `uint64_t reg_index`：单调递增的注册序号（由 `m_next_reg_index` 生成）。
  - 注册注意：
    - `Register(BaseObject*)` 会创建新的 `Entry` 并 push 到 `m_entries`，实现中没有对重复注册做去重（如果多次注册相同 `owner`，会产生多个条目）。
    - `Unregister(BaseObject*)` 会删除所有匹配 `owner` 的条目（使用 `std::remove_if`），并同时销毁并移除 `g_canvas_cache` 中对应的 canvas（若存在）。
}

, 互斥与并发 {
  - `mutable std::mutex m_mutex`：保护 `m_entries` 与 `m_callback` 的并发访问。
  - `g_canvas_cache_mutex`：保护全局 canvas 缓存 `g_canvas_cache`。
  - 实现采用短快照策略（在 `DrawAll`/`BlitAll` 中先复制 `m_entries` 的轻量快照再解锁），以缩短持锁时间并允许并发的注册/注销。
}

, 方法 `Instance()` {
  - 整体作用：返回单例 `DrawingSequence` 实例。
  - 实现：函数内静态局部变量 `static DrawingSequence inst; return inst;`（线程安全的局部静态初始化）。
}

, 方法 `Register(BaseObject* obj)` {
  - 整体作用：把 `obj` 加入绘制序列（登记以便后续 `DrawAll`/`BlitAll` 处理）。
  - 行为要点：
    - 若 `obj == nullptr` 则直接返回。
    - 在 `m_mutex` 保护下创建 `Entry` 并设置 `reg_index = m_next_reg_index++`，然后 push 到 `m_entries`。
    - 不检查重复注册——调用者若需要幂等性需在外部保证。
}

, 方法 `Unregister(BaseObject* obj)` {
  - 整体作用：从绘制序列移除 `obj` 并释放其缓存 canvas（若存在）。
  - 行为要点（实现顺序）：
    1. 先锁 `g_canvas_cache_mutex`，查找 `g_canvas_cache`，若找到并且 `canvas.id != 0` 则调用 `cf_destroy_canvas` 并把条目 erase 掉；
    2. 再锁 `m_mutex`，在 `m_entries` 中用 `std::remove_if` 删除所有 `owner == obj` 的条目。
  - 目的：保证在注销时释放 GPU/绘制资源并移除所有注册记录，避免悬空指针与资源泄露。
}

, 方法 `SetDrawCallback(DrawCallback cb)` {
  - 整体作用：设置或替换 Blit 阶段的绘制回调。
  - 实现细节：在 `m_mutex` 保护下使用 `m_callback = std::move(cb)`。
  - 语义：传入空回调是允许的；`BlitAll()` 在无回调时使用默认的基于 `BaseObject::GetPosition()` 的绘制路径。
}

, 方法 `DrawAll()`（Upload-only） {
  - 整体作用：Upload-only 阶段（每帧）：遍历注册对象，提取 `PngFrame`，为每个对象创建/更新 per-owner `CF_Canvas`，并把像素上传到 canvas 的 texture。
  - 关键实现流程：
    1. 复制 `m_entries` 的快照到本地向量（在 `m_mutex` 内完成复制后解锁）。
    2. 对快照按绘制顺序排序：先按 `owner->GetDepth()` 升序，再按 `reg_index` 升序（较小的 depth/更早的 reg_index 先处理）。
    3. 对每个 `Entry`：
       - 跳过 `owner == nullptr` 或 `!owner->IsVisible()`。
       - 通过 `owner->GetSpriteFrame()` 获取 `PngFrame`；若空或像素大小不符则跳过并记录错误（若 pixel 数据长度小于 w*h*4，会写入 `std::cerr`）。
       - 在 `g_canvas_cache` 中查找或创建 `CanvasCache`：
         - 若不存在：用 `cf_make_canvas(cf_canvas_defaults(w,h))` 新建并插入。
         - 若宽高不匹配或 `canvas.id == 0`：先销毁旧 canvas（若存在），再创建新 canvas 并更新宽高。
       - 获取目标 texture：`CF_Texture tgt = cf_canvas_get_target(local_canvas)`，计算 bytes = w*h*sizeof(CF_Pixel)，调用 `cf_texture_update(tgt, frame.pixels.data(), bytes)` 上传像素数据。
    4. 任何 canvas 创建失败（`local_canvas.id == 0`）会在 `std::cerr` 记录并跳过该对象。
  - 设计要点：
    - Upload 与最终绘制分离，允许在非渲染线程或安全时机准备像素数据。
    - 使用全局缓存以减少每帧分配开销。
    - 对像素数据长度做基本校验以防止内存/格式错误。
}

, 方法 `BlitAll()`（实际绘制） {
  - 整体作用：在渲染阶段遍历注册对象并把 per-owner canvas 绘制到场景。
  - 实现流程：
    1. 同 `DrawAll()`：复制 `m_entries` 快照并按 depth/reg_index 排序。
    2. 对每个 `Entry`：
       - 跳过 `owner == nullptr` 或 `!owner->IsVisible()`。
       - 在 `g_canvas_cache` 中查找对应 `CanvasCache`；若不存在表示尚未上传过 canvas，跳过。
       - 若 `local_canvas.id == 0` 或宽高异常，跳过。
       - 若 `m_callback` 已设置，则在 try/catch 中调用 `m_callback(local_canvas, owner, w, h)`：
         - 捕获 `std::exception` 并把 `ex.what()` 输出到 `std::cerr`；
         - 捕获其它异常并记录未知异常信息。
       - 否则使用默认绘制路径：
         - 获取对象位置 `CF_V2 pos = owner ? owner->GetPosition() : cf_v2(0.0f,0.0f)`；
         - 使用 `cf_draw_push()` / `cf_draw_translate(pos.x, pos.y)` / `cf_draw_canvas(local_canvas, cf_v2(0,0), cf_v2((float)w, (float)h))` / `cf_draw_pop()` 绘制。
  - 语义要点：
    - 回调允许用户在自定义渲染上下文中处理 canvas（例如做批处理或不同坐标变换），但应避免抛出异常；若抛出，`DrawingSequence` 会捕获并记录。
    - 默认路径以对象位置为偏移直接绘制 canvas。
}

---

## 设计要点与边界情况（摘要）
- Upload 与 Blit 分离：
  - 目的是允许在合适线程/时机上传像素数据（DrawAll），并在主渲染时刻统一绘制（BlitAll），避免渲染资源竞争与不一致帧。
- 全局缓存与资源管理：
  - `g_canvas_cache` 为每个 `BaseObject*` 保存 `CF_Canvas` 与尺寸，减少频繁分配/销毁；`Unregister` 会销毁并移除缓存，析构或路径变更应调用 `Unregister` 来释放资源。
- 排序策略：
  - 使用 `(depth, reg_index)` 作为排序键：先按 `GetDepth()` 升序，再按 `reg_index` 升序。较小 depth（或更早注册）的项先被处理。
- 注册幂等性：
  - `Register` 实现不做去重；重复注册会导致多个条目。`Unregister` 删除所有匹配条目。调用方若需幂等行为应在外部保证。
- 错误与异常：
  - 对像素大小不匹配或 canvas 创建失败会写入 `std::cerr` 并跳过该对象，避免中断整帧流程。
  - `BlitAll` 中对回调的异常进行了捕获（`std::exception` 与其它异常），以保护渲染循环的稳定性。
- 并发注意：
  - 对 `m_entries` 的访问采用短快照以减少持锁时间；对 `g_canvas_cache` 的访问使用独立互斥量以保证缓存的一致性。
  - 尽管有锁保护，`Entry::owner` 为裸指针，调用者必须保证在未 `Unregister` 前对象有效，或在对象析构时主动 `Unregister`（例如 `BaseObject` 的析构实现有调用 `DrawingSequence::Instance().Unregister(this)`）。
- 性能：
  - 频繁调用 `cf_make_canvas`/`cf_destroy_canvas` 代价较大；`g_canvas_cache` 通过复用 canvas 并只在尺寸变化时重建来降低开销。
- 可扩展性：
  - 若需按层级或批处理绘制，可扩展 `Entry`（例如添加 layer、z-index、可见遮挡标记），并在排序或 Blit 阶段使用这些字段。
