#include "down_spike.h"
#include"globalplayer.h"

void DownSpike::Start()
{
    //翻转刺的方向
    SpriteFlipY(true);
    SpriteSetSource("/sprites/Obj_Spike.png", 1);

    SetPivot(0, -1);
    SetPosition(CF_V2(position));
  
    std::vector<CF_V2> vertices = {
        { -16.0f, -16.0f },
        {  16.0f, -16.0f },
        {   0.0f, 16.0f }
    };

    SetCenteredPoly(vertices);
}

void DownSpike::OnCollisionStay(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {
    auto& g = GlobalPlayer::Instance();
    //当刺碰到玩家时销毁玩家对象
    if (other == g.Player()) {
        g.Hurt();
    }
}