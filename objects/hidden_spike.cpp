#include "hidden_spike.h"
#include "obj_manager.h"
#include "globalplayer.h"

// 声明全局帧率变量
extern int g_frame_rate; // 全局帧率，每秒帧数

void HiddenSpike::Start()
{
    // 设置默认精灵资源
    SpriteSetSource("/sprites/Obj_Spike.png", 1);
	SetDepth(-10); // 确保刺被方块遮挡

    SetPivot(0, -1);
    SetPosition(position);

    std::vector<CF_V2> vertices = {
        { -16.0f, -16.0f },
        {  16.0f, -16.0f },
        {   0.0f, 16.0f }
    };

    SetCenteredPoly(vertices);

    SetColliderType(ColliderType::LIQUID);

    // 处理旋转
    const float pi = 3.1415926535f;
    if (!direction_up) SetRotation(pi);

    // 直向移动的参数设置
    float distance = 36.0f;
    float move_time = 0.1f;   // 完成一次移动的时间（秒）

    // 传参，用于lambda表达式的捕获
    CF_V2 pos = position;
	int dir = direction_up ? 1.0f : -1.0f;

    // 清空并初始化动作序列
    m_act_seq.clear();

    m_act_seq.add(
        static_cast<int>(move_time * g_frame_rate),
        [pos, distance, dir]
        (BaseObject* obj, int current_frame, int total_frames)
        {
            if (!obj) return;

            // 计算归一化参数 [0.0, 1.0]
            float t = static_cast<float>(current_frame) / static_cast<float>(total_frames - 1);
            if (total_frames == 1) t = 1.0f;

            // 移动操作
            CF_V2 new_pos = pos + cf_v2(0.0f, dir * distance * t);
            obj->SetPosition(new_pos);
        }
    );
}

static auto& g = GlobalPlayer::Instance();
const float hw = 18.0f;

void HiddenSpike::Update()
{
	auto player = g.Player();
    if (!objs.TryGetRegisteration(g.Player())) return;
    CF_V2 player_pos = objs[player].GetPosition();
    int dir = direction_up ? 1.0f : -1.0f;
    if (once
        && position.x - player_pos.x < hw
        && player_pos.x - position.x < hw
        && (position.y - player_pos.y) * dir < 0.0f
        && (player_pos.y - position.y) * dir < 2 * hw * (check_count + 1)) {
        once = false;
        m_act_seq.play(this);
    }
}

void HiddenSpike::OnCollisionEnter(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {
    //当刺碰到玩家时销毁玩家对象
    if (other == g.Player()) {
        g.Hurt();
    }
}