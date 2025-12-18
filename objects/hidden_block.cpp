#include "hidden_block.h"
#include "globalplayer.h"

void HiddenBlock::Start()
{
    // 设置精灵属性

    SpriteSetSource("/sprites/transparent_block_.png", 1);
	SetDepth(0); // 设置渲染深度
    SetPosition(position);

    IsColliderApplyPivot(true);
    IsColliderRotate(false);

    SetPivot(-1.0f, -1.0f);
    Scale(0.5f);

    // 设置为实体碰撞类型
    SetColliderType(ColliderType::SOLID);
}

static auto& g = GlobalPlayer::Instance();

void HiddenBlock::OnCollisionEnter(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {
    //当刺碰到玩家时显形
    if (once && other == g.Player()) {
        SpriteSetSource("/sprites/block1.png", 1);
        SetPivot(-1.0f, -1.0f);
		once = false;
    }
}