#include "move_spike.h"

extern int g_frame_rate; // 全局帧率（每秒帧数）

void MoveSpike::Start() {
	// 设置精灵资源与初始状态
	SpriteSetStats("/sprites/Obj_Spike.png", 1, 1, 0);
	SetPosition(cf_v2(300.0f, 0.0f)); // 初始位置
	Scale(1.0f); // 初始缩放为 1 倍
	SetCenteredPoly({
		cf_v2(-SpriteWidth() * 0.5f, -SpriteHeight() * 0.5f),
		cf_v2(SpriteWidth() * 0.5f, -SpriteHeight() * 0.5f),
		cf_v2(0.0f, SpriteHeight() * 0.5f)
		});

	// 记录初始位置
	CF_V2 initial_pos = GetPosition();
	float move_up_distance = 150.0f;
	float move_down_distance = 300.0f;

	m_act_seq.clear();

	// 动作 0：等待0.5秒
	m_act_seq.add(
		static_cast<int>(0.5f * g_frame_rate), // 约 25 帧
		[]
		(BaseObject* obj, int current_frame, int total_frames) 
		{
			// 空动作，仅等待
		}
	);

	// 动作 1：在 0.3 秒内逐渐上移
	m_act_seq.add(
		static_cast<int>(0.3f * g_frame_rate), // 约 15 帧
		[initial_pos, move_up_distance]
		(BaseObject* obj, int current_frame, int total_frames) 
		{
			if (!obj) return;
		
			// 计算插值比例 [0.0, 1.0]
			float t = static_cast<float>(current_frame) / static_cast<float>(total_frames - 1);
			if (total_frames == 1) t = 1.0f; // 避免除零
		
			// 线性插值：从 initial_pos.y 渐变到 initial_pos.y + move_up_distance
			float current_y = initial_pos.y + move_up_distance * t;
			obj->SetPosition(cf_v2(initial_pos.x, current_y));
		}
	);

	// 动作 2：在 0.5 秒内逐渐变大至三倍
	m_act_seq.add(
		static_cast<int>(0.5f * g_frame_rate), // 约 25 帧
		[]
		(BaseObject* obj, int current_frame, int total_frames) 
		{
			if (!obj) return;
		
			float t = static_cast<float>(current_frame) / static_cast<float>(total_frames - 1);
			if (total_frames == 1) t = 1.0f;
		
			// 从 1.0 渐变到 3.0
			float current_scale = 1.0f + 2.0f * t;
			obj->Scale(current_scale);
		}
	);

	// 动作 3：在 1 秒内逐渐下移
	m_act_seq.add(
		static_cast<int>(1.0f * g_frame_rate), // 约 50 帧
		[initial_pos, move_up_distance, move_down_distance]
		(BaseObject* obj, int current_frame, int total_frames) 
		{
			if (!obj) return;
		
			float t = static_cast<float>(current_frame) / static_cast<float>(total_frames - 1);
			if (total_frames == 1) t = 1.0f;
		
			// 从 (initial_pos.y + move_up_distance) 渐变到 (initial_pos.y + move_up_distance - move_down_distance)
			float start_y = initial_pos.y + move_up_distance;
			float current_y = start_y - move_down_distance * t;
			obj->SetPosition(cf_v2(initial_pos.x, current_y));
		}
	);

	// 播放动作序列
	m_act_seq.play(this);
}

void MoveSpike::Update() {
	// 可选：在 Update 中添加其他逻辑
}