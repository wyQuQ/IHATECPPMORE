#pragma once
#include "base_object.h"
#include <vector>

class LateralSpike : public BaseObject
{
public:
    LateralSpike(CF_V2 pos) noexcept : BaseObject(), position(pos) {}
    ~LateralSpike() noexcept override {}

    // ÉúÃüÖÜÆÚ
    void Start() override;
    void OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;

private:
    CF_V2 position;
};
