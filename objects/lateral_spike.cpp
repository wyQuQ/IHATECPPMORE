#include "lateral_spike.h"
#include "globalplayer.h"

void LeftLateralSpike::Start()
{
    const double pi = 3.14159265358979323846;

    //翻转刺的方向
    SpriteSetSource("/sprites/Obj_Spike.png", 1);
    SetRotation(90.0f * pi / 180.0f);

    SetPivot(0, -1);
    SetPosition(CF_V2(position));
    std::vector<CF_V2> vertices = {
        { -16.0f, -16.0f },
        {  16.0f, -16.0f },
        {   0.0f, 16.0f }
    };

    SetCenteredPoly(vertices);
}

void RightLateralSpike::Start()
{
    const double pi = 3.14159265358979323846;

    //翻转刺的方向
    SpriteSetSource("/sprites/Obj_Spike.png", 1);
    SetRotation(270.0f * pi / 180.0f);


    SetPivot(0, -1);
    SetPosition(CF_V2(position));

    std::vector<CF_V2> vertices = {
        { -16.0f, -16.0f },
        {  16.0f, -16.0f },
        {   0.0f, 16.0f }
    };

    SetCenteredPoly(vertices);
}

void RightLateralSpike::OnCollisionStay(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {
    auto& g = GlobalPlayer::Instance();
    //当刺碰到玩家时销毁玩家对象
    if (other == g.Player()) {
        g.Hurt();
    }
}

void LeftLateralSpike::OnCollisionStay(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {
    auto& g = GlobalPlayer::Instance();
    //当刺碰到玩家时销毁玩家对象
    if (other == g.Player()) {
        g.Hurt();
    }
}