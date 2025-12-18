# I_HATE_CPP
A C++ game program using C lib and try to add OOP API for programing.Will go in trash bin one day.

---

## 文档

- 总览（Overview）：[docs/Overview.md](docs/Overview.md)

### 核心模块（快速链接）
#### 全局管理器
- `RoomLoader`：房间管理与加载 — [docs/RoomLoader.md](docs/RoomLoader.md) — 类定义与实现 `./head/room_loader.h`
- `ObjManager`：对象管理与生命周期 — [docs/ObjManager.md](docs/ObjManager.md) — 类定义 `./head/obj_manager.h`，方法实现 `./src/ObjManager.cpp`
- `ObjToken`：对象句柄与生命周期语义 — [docs/ObjToken.md](docs/ObjToken.md) — 结构定义 `./head/object_token.h`
- `DrawingSequence`：像素上传与统一绘制流水线 — [docs/DrawingSequence.md](docs/DrawingSequence.md) — 类定义 `./head/drawing_sequence.h`，上传逻辑 `./src/DrawingSequence.cpp`
- `GlobalPlayer`：全局玩家状态管理 — [docs/GlobalPlayer.md](docs/GlobalPlayer.md) — 类定义 `./head/global_player.h`，方法实现 `./src/GlobalPlayer.cpp`
#### 基类
- `BaseRoom`：房间对象基类（房间整体加载、帧更新、卸载） — [docs/BaseRoom.md](docs/BaseRoom.md) — 类定义与实现 `./head/room_loader.h`
  - 房间对象派生类：`./rooms/*.cpp`
- `BaseObject`：游戏物体基类（渲染 + 物理） — [docs/BaseObject.md](docs/BaseObject.md) — 类定义 `./head/base_object.h`，主要逻辑 `./src/base_object.cpp`、调试绘制 `./src/base_object_debug.cpp`
  - 游戏物体派生类：`./objects/*.h` 与 `./objects/*.cpp`
#### 内部物理模块
- `BasePhysics`：物理状态与形状管理 — [docs/BasePhysics.md](docs/BasePhysics.md) — 类定义 `./head/base_physics.h`
- `PhysicsSystem`：碰撞检测与事件分发 — [docs/PhysicsSystem.md](docs/PhysicsSystem.md) — 类定义 `./head/physics_system.h`，Step 实现 `./src/Collider.cpp`
