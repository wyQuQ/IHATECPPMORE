#pragma once
#include "base_object.h"

class TestBlock : public BaseObject {
public:
    TestBlock() noexcept : BaseObject() {}
    ~TestBlock() noexcept {}

    void Start() override;
    void Update() override;
};