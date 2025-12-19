#include "hidden_rotated_spike.h"
#include "obj_manager.h"
#include "globalplayer.h"

// 声明全局帧率变量
extern int g_frame_rate; // 全局帧率，每秒帧数

void HiddenRotatedSpike::Start()
{
    // 设置默认精灵资源
    SpriteSetSource("/sprites/Obj_Spike.png", 1);
	SetDepth(-10); // 确保刺被方块遮挡

    SetPivot(0, -1);
    SetPosition(position);

    std::vector<CF_V2> vertices = {
        { -15.0f, -15.0f },
        {  15.0f, -15.0f },
        {   0.0f, 15.0f }
    };

    SetCenteredPoly(vertices);

    SetColliderType(ColliderType::LIQUID);

    // 处理旋转
    const float pi = 3.1415926535f;
    if (direction_left) SetRotation(pi / 2.0f);
    else SetRotation(-pi / 2.0f);

    // 直向移动的参数设置
    float distance = 36.0f;
    float move_time = move_consumed_time;   // 完成一次移动的时间（秒）

    // 传参，用于lambda表达式的捕获
    CF_V2 pos = position;
    int attack = attack_count;
	int dir = direction_left ? 1.0f : -1.0f;

    // 清空并初始化动作序列
    m_act_seq.clear();

    m_act_seq.add(
        static_cast<int>(move_consumed_time * g_frame_rate),
        [pos, distance, dir, attack]
        (BaseObject* obj, int current_frame, int total_frames)
        {
            if (!obj) return;

            // 计算归一化参数 [0.0, 1.0]
            float t = static_cast<float>(current_frame) / static_cast<float>(total_frames - 1);
            if (total_frames == 1) t = 1.0f;

            // 移动操作
            CF_V2 new_pos = pos + cf_v2(-dir * distance * t * attack, 0.0f);
            obj->SetPosition(new_pos);
        }
    );
}

static auto& g = GlobalPlayer::Instance();
const float hh = 18.0f;

void HiddenRotatedSpike::Update()
{
	auto player = g.Player();
    if (!objs.TryGetRegisteration(g.Player())) return;
    CF_V2 player_pos = objs[player].GetPosition();
    int dir = direction_left ? 1.0f : -1.0f;
    if (once
        && position.y - player_pos.y < hh
        && player_pos.y - position.y < hh
        && (player_pos.x - position.x) * dir < 0.0f
        && (position.x - player_pos.x) * dir < 2 * hh * (check_count + 1)) {
        once = false;
        m_act_seq.play(this);
    }
}

void HiddenRotatedSpike::OnCollisionStay(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {
    //当刺碰到玩家时销毁玩家对象
    if (other == g.Player()) {
        g.Hurt();
    }
}