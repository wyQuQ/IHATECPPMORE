#pragma once
#include "base_object.h"
#include "act_seq.h"
class DiogonalLefMoveSpike : public BaseObject {
public:
    DiogonalLefMoveSpike() noexcept : BaseObject() {}
    DiogonalLefMoveSpike(CF_V2 pos) noexcept : BaseObject(), initial_position(pos) {}
    ~DiogonalLefMoveSpike() noexcept override {}

    void Start() override;
    void Update() override;
    void OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;

private:
    ActSeq m_act_seq;
    CF_V2 initial_position{ 0.0f, 0.0f };
};