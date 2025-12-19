#include "diagonal_move_spike_left.h"
#include "obj_manager.h"
#include "globalplayer.h"

// 声明全局帧率变量
extern int g_frame_rate; // 全局帧率，每秒帧数

void DiogonalLefMoveSpike::Start() {
    // 设置精灵资源和初始状态
    CF_V2 pos = initial_position;
    SpriteSetStats("/sprites/Obj_Spike.png", 1, 1, 0);
    SetPosition(pos); // 设置初始位置
    Scale(1.0f); // 初始缩放为 1 倍

    float hw = SpriteWidth() / 2.0f;
    float hh = SpriteHeight() / 2.0f;

    // 设置碰撞多边形
    std::vector<CF_V2> vertices = {
        { -hw, -hh },
        {  hw, -hh },
        { 0.0f, hh }
    };
    SetCenteredPoly(vertices);

    // 斜向移动的参数设置

    float move_distance = 100.0f; // 斜向移动的总距离
    //float diagonal_speed = 0.5f;   // 完成一次斜向移动的时间（秒）
    //float wait_time = 0.5f;        // 移动到终点后的等待时间

    // 清空并初始化动作序列
    m_act_seq.clear();

    // 动作 0: 初始等待
    m_act_seq.add(
        static_cast<int>(wait_time * g_frame_rate),
        []
        (BaseObject* obj, int current_frame, int total_frames)
        {
            // 什么都不做，只是等待
        }
    );

    // 动作 1: 斜向左上移动
    m_act_seq.add(
        static_cast<int>(diagonal_speed * g_frame_rate),
        [pos, move_distance]
        (BaseObject* obj, int current_frame, int total_frames)
        {
            if (!obj) return;

            // 计算归一化参数 [0.0, 1.0]
            float t = static_cast<float>(current_frame) / static_cast<float>(total_frames - 1);
            if (total_frames == 1) t = 1.0f;

            // 斜向左上方移动：x 和 y 同时增加
            CF_V2 new_pos = pos + cf_v2(-move_distance * t, +move_distance * t);
            obj->SetPosition(new_pos);
        }
    );

    // 动作 2: 等待
    m_act_seq.add(
        static_cast<int>(wait_time * g_frame_rate),
        []
        (BaseObject* obj, int current_frame, int total_frames)
        {
            // 等待
        }
    );

    // 动作 3: 斜向左下方移动回初始位置
    m_act_seq.add(
        static_cast<int>(diagonal_speed * g_frame_rate),
        [pos, move_distance]
        (BaseObject* obj, int current_frame, int total_frames)
        {
            if (!obj) return;

            // 计算归一化参数 [0.0, 1.0]
            float t = static_cast<float>(current_frame) / static_cast<float>(total_frames - 1);
            if (total_frames == 1) t = 1.0f;

            // 斜向左下方移动：从右上角移动回初始位置
            CF_V2 top_right_pos = pos + cf_v2(-move_distance, move_distance);
            CF_V2 new_pos = top_right_pos + cf_v2(move_distance * t, -move_distance * t);
            obj->SetPosition(new_pos);
        }
    );

    // 动作 4: 等待
    m_act_seq.add(
        static_cast<int>(wait_time * g_frame_rate),
        []
        (BaseObject* obj, int current_frame, int total_frames)
        {
            // 等待
        }
    );

    // 循环播放动作序列
    m_act_seq.play(this, true); // true 表示循环播放
}

void DiogonalLefMoveSpike::Update() {

}

void DiogonalLefMoveSpike::OnCollisionStay(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept {
    auto& g = GlobalPlayer::Instance();
    //当刺碰到玩家时销毁玩家对象
    if (other == g.Player()) {
        g.Hurt();
    }
}