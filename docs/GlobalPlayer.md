# GlobalPlayer  

## 单例管理职责  
`GlobalPlayer` 作为全局玩家状态的管理器，负责复活点/出现点记录、玩家实体的创建与销毁、以及与 `RoomLoader`/`ObjManager` 的协同。通过单例模式和轻量状态，它避免了散乱的全局变量，提供统一接口供游戏逻辑查询与控制。  

## 核心状态  
- `respawn_point` + `respawn_room`：当前复活点坐标及其所在房间，当玩家死亡或按 `R` 重生时会使用这个位置。  
- `emerge_pos`：可以额外设置一次性出现点（比如剧情过场），在下一次 `Emerge` 时生效并重置。  
- `player_token`：玩家对象在 `ObjManager` 中的令牌，用于判断实体当前是否存在并进行位置调整或销毁。  

## 接口与调用流程  
1. `SetRespawnPoint(position)`：记录复活位置与当前房间，应在玩家站定且完成初始化后调用。  
2. `Respawn()`：如果玩家实体不存在，就在记录位置创建；否则复位已有对象位置。若目标房间不是当前载入房间，会输出警告。  
3. `Emerge()`：优先使用 `emerge_pos`，否则回退到 `Respawn()`；在将实体重新放回世界后立即清除 `need_emerge` 标记。  
4. `Hurt()`：向 `ObjsManager` 请求当前玩家对象，生成大量 `Blood` 贴图模拟受伤，随后销毁玩家实体；这一流程会触发 `DrawingSequence` 与绘制子系统对新血迹的采集。  

## 使用建议  
- 游戏主循环在玩家产生/移动后保持 `GlobalPlayer::Instance().Player()` 的有效性，确保 `ObjManager` 在任意时刻能快速定位玩家。  
- 重生逻辑应先调用 `SetRespawnPoint`，再在需要的时候调用 `Respawn`/`Emerge`，以避免“位置未初始化”产生的闪烁。  
- `Hurt` 产生大量 `Blood` 对象并关闭玩家实体，用于执行玩家受伤后的视觉效果，调用后应确保游戏逻辑正确处理玩家“死亡”状态。