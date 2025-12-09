#pragma once
#include "base_object.h"
#include <iostream>
#include <cmath>
#include <unordered_map>
<<<<<<< Updated upstream

=======
>>>>>>> Stashed changes

class PlayerObject : public BaseObject {
public:
    PlayerObject() noexcept : BaseObject() {}
    ~PlayerObject() noexcept {}

    void Start() override;
    void Update() override;
<<<<<<< Updated upstream
    void OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;
	void OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;
	void OnCollisionExit(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;
=======

	void OnCollisionEnter(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept override;
	void OnCollisionStay(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept override;
	void OnCollisionExit(const ObjManager::ObjToken& other,const CF_Manifold& manifold) noexcept override;
>>>>>>> Stashed changes
};