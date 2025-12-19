#pragma once
#include "base_object.h"
#include "act_seq.h"

class DiogonalRigMoveSpike : public BaseObject {
public:
    DiogonalRigMoveSpike() noexcept : BaseObject() {}
    DiogonalRigMoveSpike(CF_V2 pos,float _diagonalspeed,float _waittime) noexcept : BaseObject(), initial_position(pos),diagonal_speed(_diagonalspeed),wait_time(_waittime) {}
    ~DiogonalRigMoveSpike() noexcept override {}

    void Start() override;
    void Update() override;
    void OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;

private:
    ActSeq m_act_seq;
    CF_V2 initial_position{ 0.0f, 0.0f };
    float diagonal_speed;
    float wait_time;
};