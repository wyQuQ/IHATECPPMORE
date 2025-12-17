#include"rotate_spike.h"

extern int g_frame_rate; // 全局帧率（每秒帧数）

void RotateSpike::Start() {

	// 设置精灵资源与初始状态
	SpriteSetStats("/sprites/Obj_Spike.png", 1, 1, 0);
	SetPosition(cf_v2(100.0f, -324.0f)); // 初始位置
	Scale(1.0f); // 初始缩放为 1 倍

	float hw = SpriteWidth() / 2.0f;
	float hh = SpriteHeight() / 2.0f;

	std::vector<CF_V2> vertices = {
		{ -hw, -hh },
		{  hw, -hh },
		{ 0.0f, hh }
	};
	SetCenteredPoly(vertices);

	// 明确设置旋转中心为精灵中心（相对值 0,0）
	SetPivot(1.0f, 0.0f);

	// 可选：设置初始角度（弧度）
	SetRotation(0.0f);

	// 记录初始位置
	CF_V2 initial_pos = GetPosition();

}

void RotateSpike::Update() {

	// 可选：在 Update 中添加其他逻辑

	const float deg = 3.0f;
	const float rad = deg * 3.14159265358979f / 180.0f;
	Rotate(rad);

}

void RotateSpike::OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {
	//当刺碰到玩家时销毁玩家对象
	if (objs[other_token].HasTag("player")) {
		objs.Destroy(other_token);
	}
}