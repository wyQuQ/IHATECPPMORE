#pragma once
#include "base_object.h"
#include <iostream>

class BlockObject : public BaseObject {
public:
    BlockObject(CF_V2 pos, bool grass) noexcept : BaseObject(), target_position(pos), with_grass(grass) {}
    ~BlockObject() noexcept {}

    void Start() override
    {
        // 设置精灵属性
        if (with_grass){
            SpriteSetSource("/sprites/block1.png", 1);
        }
        else{
            SpriteSetSource("/sprites/block2.png", 1);
        }
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
    bool with_grass = false;
};