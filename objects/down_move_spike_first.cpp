#include"down_move_spike_first.h"
#include"globalplayer.h"

extern int g_frame_rate; // 全局帧率（每秒帧数）

void FirstDownMoveSpike::Start() {

	//翻转刺的方向
	SpriteFlipY(true);
	SpriteSetSource("/sprites/Obj_Spike.png", 1);

	SetPivot(0, -1);
	SetPosition(cf_v2(414.0f,-288.0f));

	std::vector<CF_V2> vertices = {
		{ -16.0f, -16.0f },
		{  16.0f, -16.0f },
		{   0.0f, 16.0f }
	};

	SetCenteredPoly(vertices);


	// 记录初始位置
	CF_V2 initial_pos = GetPosition();
	float move_down_distance = 150.0f;



	//执行下移动作
	m_act_seq.add(
		static_cast<int>(0.2f * g_frame_rate),
		[initial_pos, move_down_distance]
		(BaseObject* obj, int current_frame, int total_frames)
		{
			if (!obj) return;

			float t = static_cast<float>(current_frame) / static_cast<float>(total_frames - 1);
			if (total_frames == 1) t = 1.0f;

			// 从 (initial_pos.y + move_up_distance) 渐变到 (initial_pos.y + move_up_distance - move_down_distance)
			float current_y = initial_pos.y - move_down_distance * t;
			obj->SetPosition(cf_v2(initial_pos.x, current_y));
		}
	);
}

void FirstDownMoveSpike::Update() {

	//获取玩家位置
	auto& player = GlobalPlayer::Instance().Player();
	if (!objs.TryGetRegisteration(player)) return;

	CF_V2 pos = objs[player].GetPosition();

	float min_x = 396.0f;
	float max_x = 432.0f;
	float min_y = -396.0f;
	float max_y = -288.0f;

	if (inside(pos, min_x, max_x, min_y, max_y)) {
		// 播放动作序列
		if(!m_act_seq.is_playing()) m_act_seq.play(this);
	}
}

void FirstDownMoveSpike::OnCollisionStay(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {
	auto& g = GlobalPlayer::Instance();
	//当刺碰到玩家时销毁玩家对象
	if (other == g.Player()) {
		g.Hurt();
	}
}