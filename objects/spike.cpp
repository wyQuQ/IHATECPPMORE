#include "spike.h"

void Spike::Start()
{
    // 设置默认精灵资源
    SpriteSetSource("/sprites/Obj_Spike.png", 1);

    SetPivot(0, -1);
    SetPosition(CF_V2(position));

	float hw = SpriteWidth() / 2.0f;
	float hh = SpriteHeight() / 2.0f;

    std::vector<CF_V2> vertices = {
        { -16.0f, 0.0f },
        {  16.0f, 0.0f },
        {   0.0f, 32.0f }
    };

    SetCenteredPoly(vertices);
}

void DownSpike::Start() 
{
    //翻转刺的方向
    SpriteFlipY();
	SpriteSetSource("/sprites/Obj_Spike.png", 1);

    SetPivot(0, -1);
	SetPosition(CF_V2(position));

    std::vector<CF_V2> vertices = {
        { -16.0f, 0.0f },
        {  16.0f, 0.0f },
        {   0.0f, 32.0f }
    };
	SetCenteredPoly(vertices);

}

void Spike::OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {
	//当刺碰到玩家时销毁玩家对象
    if (objs[other_token].HasTag("player")) {
		objs.Destroy(other_token);
    }
}

void DownSpike::OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {
    //当刺碰到玩家时销毁玩家对象
    if (objs[other_token].HasTag("player")) {
        objs.Destroy(other_token);
    }
}