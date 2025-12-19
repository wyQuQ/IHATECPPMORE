#include "vertical_moving_spike.h"
#include "obj_manager.h"
#include "globalplayer.h"

// Global frame rate
extern int g_frame_rate;

void VerticalMovingSpike::Start() {
    // Set sprite and initial state
    SpriteSetStats("/sprites/Obj_Spike.png", 1, 1, 0);
    SetPosition(initial_position);
    Scale(1.0f);

    float hw = SpriteWidth() / 2.0f;
    float hh = SpriteHeight() / 2.0f;

    //创造一个Vertical_moving_spike例子
    float temp_move_distance = move_distance;

    // Set collision shape as triangle
    std::vector<CF_V2> vertices = {
        { -16.0f, -16.0f },
        {  16.0f, -16.0f },
        {   0.0f, 16.0f }
    };
    SetCenteredPoly(vertices);

    // Vertical movement configuration
   /* float move_distance = 200.0f;*/ // Total movement distance
    //float move_speed = 0.8f;      // Time for one vertical movement (seconds)
    //float wait_time = 0.5f;       // Wait time at endpoints

    // Initialize action sequence
    m_act_seq.clear();

    // Action 0: Initial wait
    m_act_seq.add(
        static_cast<int>(wait_time * g_frame_rate),
        [](BaseObject* obj, int current_frame, int total_frames) {
            // Just waiting
        }
    );


    // Action 1: Move down
    m_act_seq.add(
        static_cast<int>(move_speed * g_frame_rate),
        [this, temp_move_distance](BaseObject* obj, int current_frame, int total_frames) {
            if (!obj) return;

            // Calculate normalized time [0.0, 1.0]
            float t = static_cast<float>(current_frame) / static_cast<float>(total_frames - 1);
            if (total_frames == 1) t = 1.0f;

            // Move down
            CF_V2 new_pos = initial_position + CF_V2(0.0f, temp_move_distance * t);
            obj->SetPosition(new_pos);
        }
    );

    // Action 2: Wait
    m_act_seq.add(
        static_cast<int>(wait_time * g_frame_rate),
        [](BaseObject* obj, int current_frame, int total_frames) {
            // Just waiting
        }
    );

    // Action 3: Move up back to initial position
    m_act_seq.add(
        static_cast<int>(move_speed * g_frame_rate),
        [this, temp_move_distance](BaseObject* obj, int current_frame, int total_frames) {
            if (!obj) return;

            // Calculate normalized time [0.0, 1.0]
            float t = static_cast<float>(current_frame) / static_cast<float>(total_frames - 1);
            if (total_frames == 1) t = 1.0f;

            // Move up
            CF_V2 bottom_pos = initial_position + CF_V2(0.0f, move_distance);
            CF_V2 new_pos = bottom_pos - CF_V2(0.0f, move_distance * t);
            obj->SetPosition(new_pos);
        }
    );

    // Action 4: Wait
    m_act_seq.add(
        static_cast<int>(wait_time * g_frame_rate),
        [](BaseObject* obj, int current_frame, int total_frames) {
            // Just waiting
        }
    );

    // Play action sequence in loop
    m_act_seq.play(this, true);
}

void VerticalMovingSpike::Update() {
    // Additional update logic can be added here
}

void VerticalMovingSpike::OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept {
    auto& g = GlobalPlayer::Instance();
    // Hurt player when collided
    if (other_token == g.Player()) {
        g.Hurt();
    }
}