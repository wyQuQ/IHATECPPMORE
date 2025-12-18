# DrawingSequence  

## 管理器化角色  
`DrawingSequence` 作为场景中所有 `BaseObject` 的绘图协调器：在注册/注销过程中维护注册列表，按照注册顺序构建有序的 `Entry` 表，并在每帧 `DrawAll()` 中负责根据对象的可见性、深度与动画状态统一采集要展示的 `CF_Sprite`。它用单例+`std::mutex` 保护来保证主循环、重生流程与可能的其他线程在并发操作对象列表时不会出现竞态。  

### 生命周期操作  
- `Register` 为每个对象分配一个增量的 `reg_index`，防止排序依赖于 `std::vector` 的内存地址；重复注册会被检测并略过。  
- `Unregister` 通过查找 `Entry`、移除 `BaseObject` 引用并释放记录，同时清理内存、保持序列长度合理。  

## 绘制提交逻辑  
1. `DrawAll()` 先加锁、重置 `last_image_id` 及 `s_pending_sprites` 缓存，确保每帧上下文干净。  
2. 按深度 + `reg_index` 对活跃对象排序，保持渲染顺序确定性。  
3. 每个可见对象：更新动画、同步位置、触发 UI 形状/碰撞回调，并调用 `PushFrameSprite()` 生成 `spritebatch_sprite_t`。  `PushFrameSprite` 使用当前 `s_draw->mvp` 计算几何，累积到 `s_pending_sprites`，当缓存达到 `kSpriteChunkSize` 时就通过 `FlushPendingSprites()` 封装为一个新的 `CF_Command`。  
4. `FlushPendingSprites()` 会在 `s_pending_sprites` 非空时创建 `CF_Command`、将条目逐个写入 `cmd.items`，然后清空缓存，为下一帧或下一个批次做好准备。  
5. 帧遍历完毕后再次调用 `FlushPendingSprites()`，确保残留条目被提交；最终，`app_draw_onto_screen` 会读取 `s_draw->cmds`，由 Cute 渲染管线遍历 `cmd.items` 并最终向屏幕提交图元。  

## 设计要点  
- 中间缓存防止 `Cute::Array` 因瞬时大量 `DRAW_PUSH_ITEM` 扩容导致的内存暴涨，同时保留 `cf_draw`/`cam_stack` 所需的 `s_draw` 命令结构。  
- 缓存批量提交使得即便同一帧渲染成千上万的 sprite，也只会在 `s_draw->cmds` 中生成有限数量的 `CF_Command`，每条 `cmd.items` 的容量可控。  
- 该流程仍兼容 Cute 的渲染回调，`DrawingSequence` 仅负责上传与提交，最终绘制由 `app_draw_onto_screen` 或其他渲染器执行。