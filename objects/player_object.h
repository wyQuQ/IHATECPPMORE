#pragma once
#include "base_object.h"
#include <iostream>
#include <cmath>
#include <unordered_map>

#include "debug_config.h"

class PlayerObject : public BaseObject {
public:
    PlayerObject() noexcept : BaseObject() {}
    ~PlayerObject() noexcept {}

    void Start() override;
    void Update() override;
	void EndFrame() override;

    void Exclusion(const CF_Manifold& m);

	void OnCollisionEnter(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;
	void OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;
	void OnCollisionExit(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;
private:
	bool grounded = false;
	float hold_time_left = 0.0f;
	int coyote_time_left = 0;
	int jump_count = 2;
};

#if TESTER
class Tester : public BaseObject {
	public:
	Tester() noexcept : BaseObject() {}
	~Tester() noexcept {}
	void Start() override;
	void Update() override;
	float tspeed = 1.6f;
};
#endif