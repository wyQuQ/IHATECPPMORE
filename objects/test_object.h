#pragma once
#include "base_object.h"

class TestObject : public BaseObject {
public:
    TestObject() noexcept : BaseObject() {}
    ~TestObject() noexcept {}

    void Start() override;
    void Update() override;
};