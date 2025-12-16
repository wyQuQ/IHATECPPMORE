# DrawingSequence

## 概述  
`DrawingSequence` 仍然负责对象注册/注销与每帧的纹理上传，但现在仅保留上传阶段，真正的绘制由外部渲染线程或系统自行执行。

## 行为与接口
- `Register(BaseObject*)`/`Unregister(BaseObject*)` 控制对象在序列中的生命周期；注册时会为 `Entry` 分配递增的 `reg_index` 以保持 deterministic 排序，注销则自动回收关联资源。  
- `DrawAll()` 是唯一的每帧方法：绑定的渲染器（通过 `DrawCallback`）会在此阶段针对每个 `BaseObject` 的 `CF_Sprite` 提取当前帧像素并上传到 per-owner canvas，未在此处执行最终 `Blit`。  
- 类内部通过 `std::mutex` 保护 `m_entries` 及其他共享状态，使得上传阶段可以与注册/注销并发安全地共存。

## 使用建议
- 确保每个需要渲染的 `BaseObject` 在加载完 sprite 后注册，在销毁前注销。  
- 主循环应在更新阶段结束后调用 `DrawingSequence::Instance().DrawAll()`，上传完成后由渲染线程直接读取 canvas 进行绘制（`BlitAll()` 被移除）。  
- `DrawCallback` 可绑定具体绘制逻辑，在上传完成后由外部自行触发最终呈现。