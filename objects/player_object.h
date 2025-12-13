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
	void StartFrame() override;
	void EndFrame() override;

	void OnExclusionSolid(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;
private:
	bool grounded = false;
	bool double_jump_ready = true;
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