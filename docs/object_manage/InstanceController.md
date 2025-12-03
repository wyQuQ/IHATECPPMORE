# InstanceController

## 目录
- [概述](#概述)
- [类 `InstanceController`](#类-instancecontroller)
  - [属性 `objects_`](#属性-objects_)
  - [属性 `free_indices_`](#属性-free_indices_)
  - [属性 `object_index_map_`](#属性-object_index_map_)
  - [属性 `pending_destroys_`](#属性-pending_destroys_)
  - [属性 `pending_destroy_set_`](#属性-pending_destroy_set_)
  - [属性 `pending_creates_`](#属性-pending_creates_)
  - [属性 `alive_count_`](#属性-alive_count_)
  - [方法 `Instance()`](#方法-instance)
  - [方法 `CreateImmediate<T, Args...>(Args&&... args)`](#方法-createimmediatet-args-args-args)
  - [方法 `CreateDelayed<T, Args...>(Args&&... args)`](#方法-createdelayedt-args-args-args)
  - [方法 `CreateImmediateFromUniquePtr(std::unique_ptr<BaseObject> obj)`](#方法-createimmediatefromuniqueptrstduniqueptrbaseobject-obj)
  - [方法 `CreateDelayedFromFactory(std::function<std::unique_ptr<BaseObject>()> factory)`](#方法-createdelayedfromfactorystdfunctionstduniqueptrbaseobject-factory)
  - [方法 `ReserveSlotForCreate()`](#方法-reserveslotforcreate)
  - [方法 `Get(const ObjectToken& token)`](#方法-getconst-objecttoken-token)
  - [方法 `GetAs<T>(const ObjectToken& token)`](#方法-getast-const-objecttoken-token)
  - [方法 `IsValid(const ObjectToken& token) const`](#方法-isvalidconst-objecttoken-token-const)
  - [方法 `DestroyEntryImmediate(uint32_t index)`](#方法-destroyentryimmediateuint32t-index)
  - [方法 `DestroyImmediate(BaseObject* ptr)`](#方法-destroyimmediatebaseobject-ptr)
  - [方法 `DestroyImmediate(const ObjectToken& token)`](#方法-destroyimmediateconst-objecttoken-token)
  - [方法 `DestroyDelayed(BaseObject* ptr)`](#方法-destroydelayedbaseobject-ptr)
  - [方法 `DestroyDelayed(const ObjectToken& token)`](#方法-destroydelayedconst-objecttoken-token)
  - [方法 `DestroyAll()`](#方法-destroyall)
  - [方法 `UpdateAll()`](#方法-updateall)
  - [方法 `Count()`](#方法-count)
- [设计要点与边界情况（摘要）](#设计要点与边界情况摘要)

该文档详尽介绍 `InstanceController` 类的属性、结构原理与每个方法的参数与实现细节。

---

类 `InstanceController` {
- 属性 `objects_` {
  - 作用：按索引存储受管理对象的槽（`Entry`）。外部通过 `ObjectToken.index` 定位到对应槽以访问对象。
  - 结构与原理：
    - 类型：`std::vector<Entry>`。
    - `Entry` 包含：
      - `std::unique_ptr<BaseObject> ptr`：对象的唯一所有权，由控制器管理生命周期。
      - `uint32_t generation`：世代号，用于令牌失效（避免槽复用造成的 ABA 问题）。
      - `bool alive`：表示槽当前是否持有活跃对象。
    - 工作流程：创建时把对象移入 `ptr` 并设置 `alive = true`；销毁时调用 `OnDestroy()`、`ptr.reset()`、`alive = false` 并 `++generation`，随后将槽索引回收到 `free_indices_`。
}
- 属性 `free_indices_` {
  - 作用：保存可重用槽索引以减少分配与碎片，提升性能。
  - 结构与原理：`std::vector<uint32_t>`，以栈（LIFO）方式使用（`push_back`/`pop_back`），优先复用已分配槽而不是扩展 `objects_`。
}
- 属性 `object_index_map_` {
  - 作用：从 `BaseObject*` 快速定位其在 `objects_` 中的索引，加速按指针查找与销毁。
  - 结构与原理：`std::unordered_map<BaseObject*, uint32_t>`。在创建时插入，在销毁时擦除。若映射未命中，代码会回退线性扫描以保证兼容性。
}
- 属性 `pending_destroys_` {
  - 作用：延迟销毁队列，存放将在下一次 `UpdateAll()` 的删除阶段执行实际销毁的 `ObjectToken`。
  - 结构与原理：`std::vector<ObjectToken>`，通过批处理避免在不安全上下文（如回调）立即删除对象。
}
- 属性 `pending_destroy_set_` {
  - 作用：对 `pending_destroys_` 做去重，防止同一对象被重复入队。
  - 结构与原理：`std::unordered_set<uint64_t>`，使用组合键 `(uint64_t(index) << 32) | generation`，结合 `generation` 可避免旧待删记录在槽复用后错误匹配。
}
- 属性 `pending_creates_` {
  - 作用：延迟创建队列，保存目标槽索引与工厂函数，实际构造在 `UpdateAll()` 的创建阶段执行。
  - 结构与原理：`std::vector<PendingCreate>`，其中 `PendingCreate` 包含 `uint32_t index` 与 `std::function<std::unique_ptr<BaseObject>()> factory`。`CreateDelayed` 将构造参数捕获为 `std::tuple` 并封装为 `factory`，真实构造在 `UpdateAll()` 中执行以保证帧边界一致性。
}
- 属性 `alive_count_` {
  - 作用：记录当前活跃对象数量，便于快速查询而不需遍历 `objects_`。
  - 结构与原理：`size_t`，创建时 `++alive_count_`，销毁时 `--alive_count_`（带边界保护）。
}

, 方法 `Instance()` {
  - 整体作用：返回单例 `InstanceController` 实例。
  - 参数及其作用：无。
  - 实现方式：函数内静态局部变量 `static InstanceController inst; return inst;`，利用 C++11+ 的局部静态安全初始化做惰性构造（注意：类其它方法未提供并发保护）。
}

, 方法 `CreateImmediate<T, Args...>(Args&&... args)` {
  - 整体作用：立即构造类型 `T`（必须派生自 `BaseObject`），控制器接管所有权并立即调用 `Start()`，返回 `ObjectToken`。
  - 参数及其作用：
    - 模板 `T`：目标类型（派生自 `BaseObject`）。
    - `Args&&... args`：转发到 `T` 的构造函数。
  - 实现方式：头文件模板使用 `static_assert` 验证类型，`std::make_unique<T>(std::forward<Args>(args)...)` 创建对象并将 `unique_ptr` 传递给非模板实现 `CreateImmediateFromUniquePtr()`，由其完成槽分配、`generation` 管理、映射更新与 `Start()` 调用。
}

, 方法 `CreateDelayed<T, Args...>(Args&&... args)` {
  - 整体作用：延迟创建：预留槽并返回 token，实际构造与 `Start()` 在随后 `UpdateAll()` 的创建阶段执行。
  - 参数及其作用：
    - 模板 `T`：目标类型（派生自 `BaseObject`）。
    - `Args&&... args`：构造参数，会被捕获并在工厂中展开转发。
  - 实现方式：将参数打包为 `std::tuple`，创建 `factory` lambda（使用 `std::apply` 展开），把 `factory` 传递给 `CreateDelayedFromFactory()`。该函数调用 `ReserveSlotForCreate()`、提前 `++generation` 并将 `PendingCreate` 入队 `pending_creates_`。
}

, 方法 `CreateImmediateFromUniquePtr(std::unique_ptr<BaseObject> obj)` {
  - 整体作用：非模板实现：把已构造的 `BaseObject`（`unique_ptr`）放入槽并立即 `Start()`，返回对应 `ObjectToken`。
  - 参数及其作用：`obj`：所有权将被移交的 `unique_ptr<BaseObject>`。
  - 实现方式：若 `obj == nullptr` 返回无效 token；调用 `ReserveSlotForCreate()` 得到 `index`，移动 `obj` 至 `objects_[index].ptr`，设 `e.alive = true`、`++e.generation`、记录 `object_index_map_`、`++alive_count_`，调用 `Start()`，返回 `{index, e.generation}`。日志通过 `std::cerr` 输出。
}

, 方法 `CreateDelayedFromFactory(std::function<std::unique_ptr<BaseObject>()> factory)` {
  - 整体作用：非模板实现：为延迟创建预留槽、提前 bump `generation` 并将工厂入队，返回 token。
  - 参数及其作用：`factory`：将在 `UpdateAll()` 被调用以创建对象的工厂函数。
  - 实现方式：若 `factory` 为空返回无效 token；调用 `ReserveSlotForCreate()` 得到 `index`，`++e.generation`（提前 bump），将 `PendingCreate{index, std::move(factory)}` 推入 `pending_creates_` 并返回 token。若工厂在 `UpdateAll()` 中失败（抛异常或返回 `nullptr`），实现会再次 `++generation` 并回收槽。
}

, 方法 `ReserveSlotForCreate()` {
  - 整体作用：分配或复用一个槽索引供创建使用（不负责 `generation` bump）。
  - 参数及其作用：无。
  - 实现方式：若 `free_indices_` 非空，弹出一个索引并清理该 `Entry`（`ptr.reset()`、`alive=false`），否则在 `objects_` 尾部 `emplace_back()` 新 `Entry` 并返回新索引。
}

, 方法 `Get(const ObjectToken& token)` {
  - 整体作用：通过 `ObjectToken` 获取对应 `BaseObject*`；若 token 无效或对象不再存活返回 `nullptr`。
  - 参数及其作用：`token`（包含 `index` 与 `generation`）。
  - 实现方式：越界检查 `token.index`，验证 `e.alive && e.generation == token.generation`，返回 `e.ptr.get()` 或 `nullptr`。使用 `generation` 防止槽复用后的误访问。
}

, 方法 `GetAs<T>(const ObjectToken& token)` {
  - 整体作用：模板快捷方法：返回 `static_cast<T*>(Get(token))`。
  - 参数及其作用：模板 `T`：目标派生类型；`token` 同上。
  - 实现方式：内联 `static_cast`，不做运行时类型检查（调用方须保证类型一致）。
}

, 方法 `IsValid(const ObjectToken& token) const` {
  - 整体作用：判断 `token` 是否仍然有效并指向活跃对象。
  - 参数及其作用：`token`。
  - 实现方式：越界检查、`alive`、`generation` 与 `ptr` 非空校验，返回布尔值。
}

, 方法 `DestroyEntryImmediate(uint32_t index)` {
  - 整体作用：内部立即销毁指定槽的对象（调用 `OnDestroy()`、擦除映射、释放 `unique_ptr`、bump generation、回收槽、更新计数）。
  - 参数及其作用：`index`。
  - 实现方式：若越界或非活跃直接返回；调用 `e.ptr->OnDestroy()`，`object_index_map_.erase(raw)`，`e.ptr.reset()`，`e.alive = false`，`++e.generation`，`free_indices_.push_back(index)`，并 `--alive_count_`（有界检查）。
}

, 方法 `DestroyImmediate(BaseObject* ptr)` {
  - 整体作用：通过指针立即销毁对象（若对象由控制器管理）。
  - 参数及其作用：`ptr`。
  - 实现方式：若 `ptr == nullptr` 返回；尝试在 `object_index_map_` 中查找并双重检验指针；若未命中回退线性扫描；若找到调用 `DestroyEntryImmediate(index)`，否则记录日志。
}

, 方法 `DestroyImmediate(const ObjectToken& token)` {
  - 整体作用：通过 `ObjectToken` 立即销毁对象（若 token 有效）。
  - 参数及其作用：`token`。
  - 实现方式：越界与 token 校验后调用 `DestroyEntryImmediate(token.index)`；无效时记录日志并返回。
}

, 方法 `DestroyDelayed(BaseObject* ptr)` {
  - 整体作用：按指针标记延迟销毁，实际销毁在下一次 `UpdateAll()` 的删除阶段执行。
  - 参数及其作用：`ptr`。
  - 实现方式：通过 `object_index_map_` 或线性扫描定位索引，转换为 `ObjectToken` 并调用 `DestroyDelayed(token)`；找不到则记录日志。
}

, 方法 `DestroyDelayed(const ObjectToken& token)` {
  - 整体作用：将 `token` 加入延迟销毁队列（去重），等待 `UpdateAll()` 执行实际销毁。
  - 参数及其作用：`token`。
  - 实现方式：越界与匹配检查后计算键 `key = (uint64_t(index) << 32) | generation`，尝试插入 `pending_destroy_set_` 做去重；若插入成功则 `pending_destroys_.push_back(token)`。
}

, 方法 `DestroyAll()` {
  - 整体作用：立即销毁并清理控制器管理的所有对象与延迟队列（用于退出/重置）。
  - 参数及其作用：无。
  - 实现方式：记录日志，清空 `pending_creates_`、`pending_destroys_`、`pending_destroy_set_`；遍历 `objects_` 对活跃条目调用 `OnDestroy()`、擦除映射、释放 `ptr`、`e.alive = false`、`++e.generation` 并回收索引；最后清空容器并把 `alive_count_ = 0`。
}

, 方法 `UpdateAll()` {
  - 整体作用：每帧驱动：按顺序执行每帧更新、处理延迟删除、处理延迟创建。
  - 参数及其作用：无。
  - 实现方式（三阶段）：
    1. 遍历 `objects_`，对每个 `e.alive && e.ptr` 调用 `e.ptr->FramelyUpdate()`；
    2. 处理 `pending_destroys_`：验证 token 并对有效项调用 `DestroyEntryImmediate()`，之后清空 `pending_destroys_` 与 `pending_destroy_set_`；
    3. 处理 `pending_creates_`：对每个 `PendingCreate` 使用 `try/catch` 调用 `factory()`；若成功把返回的 `unique_ptr` 移入槽并 `Start()`，否则 `++e.generation` 并回收槽；处理完成后清空 `pending_creates_`。
  - 关键细节：创建阶段发生在删除之后，避免延迟创建覆盖已标记删除的槽；工厂调用被异常捕获以保证稳定性。
}

, 方法 `Count()` {
  - 整体作用：返回当前活跃对象数量。
  - 实现方式：内联返回 `alive_count_`。
}

}

---

## 设计要点与边界情况（摘要）
- 使用 `index + generation` 的 token 机制避免槽复用（ABA）问题；只有在 `objects_[index].alive == true` 且 `objects_[index].generation == token.generation` 时 token 有效。
- 延迟创建/销毁用于保证帧边界一致性并避免在回调中进行不安全的即时创建/销毁。
- 对象所有权由 `std::unique_ptr<BaseObject>` 管理；控制器负责调用 `Start()`/`OnDestroy()` 并释放资源。
- 实现未提供线程安全；若需多线程访问，应在外部添加同步（如互斥）。
- 日志和错误通过 `std::cerr` 输出，便于调试工厂异常或无效 token 情况。