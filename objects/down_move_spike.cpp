#include"down_move_spike.h"

extern int g_frame_rate; // 全局帧率（每秒帧数）

void DownMoveSpike::Start() {

	// 设置精灵资源与初始状态
	SpriteSetStats("/sprites/Obj_Spike.png", 1, 1, 0);
	SetPosition(cf_v2(50.0f, -174.0f)); // 初始位置
	Scale(1.0f); // 初始缩放为 1 倍

	float hw = SpriteWidth() / 2.0f;
	float hh = SpriteHeight() / 2.0f;

	std::vector<CF_V2> vertices = {
		{ -hw, -hh },
		{  hw, -hh },
		{ 0.0f, hh }
	};
	SetCenteredPoly(vertices);

	// 记录初始位置
	CF_V2 initial_pos = GetPosition();
	float move_down_distance = 150.0f;

	//执行下移动作
	m_act_seq.add(
		static_cast<int>(1.0f * g_frame_rate), // 约 60 帧
		[initial_pos, move_down_distance]
		(BaseObject* obj, int current_frame, int total_frames)
		{
			if (!obj) return;

			float t = static_cast<float>(current_frame) / static_cast<float>(total_frames - 1);
			if (total_frames == 1) t = 1.0f;

			// 从 (initial_pos.y + move_up_distance) 渐变到 (initial_pos.y + move_up_distance - move_down_distance)
			float start_y = initial_pos.y + move_down_distance;
			float current_y = start_y - move_down_distance * t;
			obj->SetPosition(cf_v2(initial_pos.x, current_y));
		}
	);

	// 播放动作序列
	m_act_seq.play(this);

}

void DownMoveSpike::Update() {
	// 可选：在 Update 中添加其他逻辑
}

void DownMoveSpike::OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {
	//当刺碰到玩家时销毁玩家对象
	if (objs[other_token].HasTag("player")) {
		objs.Destroy(other_token);
	}
}