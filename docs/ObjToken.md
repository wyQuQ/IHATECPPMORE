# ObjToken

## 概述
`ObjToken` 是 `ObjManager` 分发给每个 `BaseObject` 的轻量句柄，携带 `(index, generation, isRegitsered)` 三要素以支持 pending 与已注册对象的安全访问。创建后会被写入对象自身（`BaseObject::SetObjToken`）并保存于 `ObjManager` 的映射中，供外部在帧间通过 `ObjManager` 访问或销毁对象。

## 字段说明
- `index`：指向 `objects_` 或 pending 区的 slot／pending id；`std::numeric_limits<uint32_t>::max()` 表示无效 token。
- `generation`：与 slot 中 `Entry::generation` 同步，`ObjManager` 每次销毁后会自增以令旧 token 失效；pending token 使用独立的 pending id，不直接对比 generation。
- `isRegitsered`：true 表示 `index` 是已注册对象的 `objects_` 下标，可以直接用于 `operator[]`；false 表示此 token 仍在 `pending_creates_`，需要 `TryGetRegisteration` 升级成真实 token 或通过 `operator[]` 特殊路径访问 pending 对象。

## 与 ObjManager/ BaseObject 的协作
- `ObjManager::Create` 返回的 token 初始化为 pending，函数会在返回前把对象放入 `pending_creates_` 并执行 `BaseObject::Start()`；此时 BaseObject 内部的 `m_obj_token` 仍为 Invalid，直到 `UpdateAll` 提交完成后才通过 `SetObjToken` 写入真实 token。
- `ObjManager::TryGetRegisteration` 负责将 pending token（`isRegitsered==false`）换成注册 token，非 const 重载会替换输入值以便继续使用 `operator[]` 访问 `BaseObject`；对于已注册 token，它会验证 index/generation/alive 并在无效时清空。
- 通过 `ObjManager::operator[]` 访问 `BaseObject` 会根据 token 类型：pending 直接读取 `pending_creates_`，注册 token 则验证 slot 并返回指向 `objects_[index]` 的对象引用（必要时抛出 `std::out_of_range`）。
- `ObjManager::IsValid` 仅对已注册 token 生效（`isRegitsered==true`），检查 generation/alive/pointer 是否仍然匹配，不能用 pending token 查询当前状态。
- `ObjManager::Destroy` 与 `DestroyAll` 依赖 ObjToken 来决定是否将 `BaseObject::OnDestroy()` 异步执行，并在销毁后令对应 token 失效（`generation` 自增、`alive=false`）；`BaseObject::OnDestroy` 可根据 `GetObjToken()` 获取自己的 token 做额外逻辑。

## 使用建议
- 跨帧持有 `ObjToken` 时，总是在操作前调用 `TryGetRegisteration` 或 `IsValid` 以确认对象仍然有效，避免持有 pending id 的 token 被错误当作已注册处理。
- 使用 `operator==/!=` 比较 token，避免直接比较 `index`（因为多次销毁/创建可能复用 slot 但 generation 不同）。
- 在需要让 BaseObject 自己感知 token 时，可通过 `ObjManager::Create` 后待下帧 `UpdateAll` 提交后再调用 `BaseObject::GetObjToken`（受保护、派生类才能调用）。
- `ObjToken::Invalid()` 是构造默认状态的安全起点，适用于未初始化或销毁后的 token。