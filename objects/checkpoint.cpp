// ---
// 待完善
// ---
#include "checkpoint.h"
#include "globalplayer.h"

void Checkpoint::Start()
{
    // 把对象放到传入的位置
    SetPosition(position);

    // 标记为 checkpoint，便于 ObjManager 的 FindTokensByTag 查找 
    AddTag("checkpoint");

    // 不作为实体阻挡，但需要能够接收“被击中”事件 -> 使用 LIQUID 以便触发碰撞回调同时不阻挡
    SetColliderType(ColliderType::LIQUID);

	// 设置精灵资源（静态贴图）
     SpriteSetSource("/sprites/Save_red.png", 1);
     SpriteSetUpdateFreq(1);
}

// 碰撞回调：如果checkpoint被子弹击中，则将该 checkpoint 设为当前的激活复活点（仅允许一个激活点），最后销毁子弹
void Checkpoint::OnCollisionEnter(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept
{
    try {
        if (!other.isValid()) return;
        // 确保 other 为已注册对象
        if (!objs.IsValid(other)) return;

        // 只响应打到带有 "bullet" 标签的对象
        // （之后可以继续完善，比如添加音效反馈，对其它类型物体产生效果等）
        if (!objs[other].HasTag("bullet")) return;

        // 如果自己 不是 已经激活的 checkpoint,进行操作。（若是，则不进行任何操作）
		// 仅允许一个 checkpoint 处于激活状态
        if (!HasTag("checkpoint_active")) {

            // 查找已有激活的 checkpoint（若存在则取消激活并切回红色贴图）
            ObjManager::ObjToken prev = objs.FindTokensByTag("checkpoint_active");
            if (prev.isValid() && objs.IsValid(prev)) {
                try {
                    objs[prev].SpriteSetSource("/sprites/Save_red.png", 1);
                    objs[prev].RemoveTag("checkpoint_active");
                }
                catch (...) {
                    // 忽略局部错误，保持健壮性
                }
            }

            // 将当前被击中的 checkpoint 设为激活状态（绿色贴图 + 标签）
            try {
                SpriteSetSource("/sprites/Save_green.png", 1);
                AddTag("checkpoint_active");
            }
            catch (...) {
                // 忽略局部错误
            }
        }

        // 销毁子弹
		objs.Destroy(other);
    }
    catch (...) {
        // 保证 noexcept
    }
}

