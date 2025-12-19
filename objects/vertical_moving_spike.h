#pragma once
#include "base_object.h"
#include "act_seq.h"

class VerticalMovingSpike : public BaseObject {
public:
    VerticalMovingSpike(CF_V2 pos,float _move_speed,float _wait_time,float _move_distance) noexcept : BaseObject(), initial_position(pos),move_speed(_move_speed),wait_time(_wait_time),move_distance(_move_distance) {}
    ~VerticalMovingSpike() noexcept override {}

    void Start() override;
    void Update() override;
    void OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;

private:
    ActSeq m_act_seq;
    CF_V2 initial_position{ 0.0f, 0.0f };
    float move_distance;
    float move_speed ;      // Time for one vertical movement (seconds)
    float wait_time ;       // Wait time at endpoints
};