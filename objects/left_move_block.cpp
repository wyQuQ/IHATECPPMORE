#include "left_move_block.h"
#include "obj_manager.h"
#include "globalplayer.h"

// 全局帧率（每秒帧数）
extern int g_frame_rate;

void LeftMoveBlock::Start() {
    //图片设置
    SpriteSetStats("/sprites/block1.png", 1, 1, 0);
    SetPosition(initial_position);
    Scale(1.0f);
    ScaleX(2.0f);
    ScaleY(0.5f);

    float hw = SpriteWidth() / 2.0f;
    float hh = SpriteHeight() / 2.0f;


    SetCenteredAabb(hw, hh);
    SetColliderType(ColliderType::SOLID);


    float move_distance = 260.0f;
    float move_speed = 1.7f;
    float wait_time = 1.3f;

    m_act_seq.clear();

    m_act_seq.add(
        static_cast<int>(wait_time * g_frame_rate),
        [](BaseObject* obj, int current_frame, int total_frames) {

        }
    );

    m_act_seq.add(
        static_cast<int>(move_speed * g_frame_rate),
        [this, move_distance](BaseObject* obj, int current_frame, int total_frames) {
            if (!obj) return;

            // [0.0, 1.0]
            float t = static_cast<float>(current_frame) / static_cast<float>(total_frames - 1);
            if (total_frames == 1) t = 1.0f;

            CF_V2 new_pos = initial_position - CF_V2(move_distance * t, 0.0f);
            obj->SetPosition(new_pos);
        }
    );


    m_act_seq.add(
        static_cast<int>(wait_time * g_frame_rate),
        [](BaseObject* obj, int current_frame, int total_frames) {

        }
    );


    m_act_seq.add(
        static_cast<int>(move_speed * g_frame_rate),
        [this, move_distance](BaseObject* obj, int current_frame, int total_frames) {
            if (!obj) return;

            // [0.0, 1.0]
            float t = static_cast<float>(current_frame) / static_cast<float>(total_frames - 1);
            if (total_frames == 1) t = 1.0f;


            CF_V2 right_pos = initial_position - CF_V2(move_distance, 0.0f);
            CF_V2 new_pos = right_pos + CF_V2(move_distance * t, 0.0f);
            obj->SetPosition(new_pos);
        }
    );


    m_act_seq.add(
        static_cast<int>(wait_time * g_frame_rate),
        [](BaseObject* obj, int current_frame, int total_frames) {

        }
    );


    m_act_seq.play(this, true);
}

void LeftMoveBlock::Update() {

}

void LeftMoveBlock::OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {
    auto& g = GlobalPlayer::Instance();
}