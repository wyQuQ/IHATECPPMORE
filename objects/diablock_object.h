#pragma once
#include "base_object.h"
#include <iostream>

class DiaBlockObject : public BaseObject {
public:
    DiaBlockObject(CF_V2 pos) noexcept : BaseObject(), target_position(pos){}
    ~DiaBlockObject() noexcept {}

    void Start() override
    {
        
            SpriteSetSource("/sprites/diablock.png", 1);
        
        SetDepth(0); // 设置渲染深度
        SetPosition(target_position);
        IsColliderApplyPivot(true);
        IsColliderRotate(false);
        SetPivot(-1.0f, -1.0f);
        Scale(0.5f);

        // 设置为实体碰撞类型
        SetColliderType(ColliderType::SOLID);
    }
private:
    CF_V2 target_position{ 0.0f, 0.0f };
};