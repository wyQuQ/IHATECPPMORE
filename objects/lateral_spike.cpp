#include "lateral_spike.h"

void LateralSpike::Start()
{
	const double pi = 3.14159265358979323846;

    //翻转刺的方向
    SpriteSetSource("/sprites/Obj_Spike.png", 1);
    SetRotation(90.0f * pi / 180.0f);


    SetPivot(1, 0);
    SetPosition(CF_V2(position));

    std::vector<CF_V2> vertices = {
        { -16.0f, 0.0f },
        {  16.0f, 0.0f },
        {   0.0f, 32.0f }
    };

    SetCenteredPoly(vertices);
}

void LateralSpike::OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {
    //当刺碰到玩家时销毁玩家对象
    if (objs[other_token].HasTag("player")) {
        objs.Destroy(other_token);
    }
}