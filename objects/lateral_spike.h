#pragma once
#include "base_object.h"
#include <vector>

class LeftLateralSpike : public BaseObject
{
public:
    LeftLateralSpike(CF_V2 pos) noexcept : BaseObject(), position(pos) {}
    ~LeftLateralSpike() noexcept override {}

    // 生命周期
    void Start() override;
    void OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;

private:
    CF_V2 position;
};

class RightLateralSpike : public BaseObject
{
public:
    RightLateralSpike(CF_V2 pos) noexcept : BaseObject(), position(pos) {}
    ~RightLateralSpike() noexcept override {}

    // 生命周期
    void Start() override;
    void OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;

private:
    CF_V2 position;
};
