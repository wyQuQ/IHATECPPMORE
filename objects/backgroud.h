#pragma once
#include "base_object.h"

class Backgroud : public BaseObject {
public:
    Backgroud() noexcept : BaseObject() {}
    ~Backgroud() noexcept {}

    void Start() override;
};