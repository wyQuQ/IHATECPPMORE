#pragma once
#include "base_object.h"
#include <vector>

class DownSpike : public BaseObject
{
public:

    DownSpike(CF_V2 pos) noexcept : BaseObject(), position(pos) {}
    ~DownSpike() noexcept override {}

    // ÉúÃüÖÜÆÚ
    void Start() override;
    void OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;

private:

    CF_V2 position;

};
