#pragma once
#include "base_object.h"
#include <iostream>
#include <cmath>
#include <unordered_map>

class PlayerObject : public BaseObject {
public:
    PlayerObject() noexcept : BaseObject() {}
    ~PlayerObject() noexcept {}

    void Start() override;
    void Update() override;
};