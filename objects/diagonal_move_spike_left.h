#pragma once
#include "base_object.h"
#include "act_seq.h"
#include <vector>

class DiogonalLefMoveSpike : public BaseObject {
public:
    DiogonalLefMoveSpike() noexcept : BaseObject() {}
    DiogonalLefMoveSpike(CF_V2 pos, float _diagonalspeed, float _waittime) noexcept : BaseObject(), initial_position(pos), diagonal_speed(_diagonalspeed), wait_time(_waittime) {}
    ~DiogonalLefMoveSpike() noexcept override {}

    void Start() override;
    void Update() override;
    void OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;

private:
    ActSeq m_act_seq;
    CF_V2 initial_position{ 0.0f, 0.0f };
    float diagonal_speed;
    float wait_time;
};