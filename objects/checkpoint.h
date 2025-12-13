#pragma once
#include "base_object.h"

// Checkpoint：可标记的复活点对象。
// - 构造时可传入一个位置（CF_V2）。Start() 会把对象放到该位置并打上标签 "checkpoint"。
// - 作为简单标记对象，不参与物理碰撞（ColliderType::VOID）并可选择性显示静态精灵/调试轮廓。
class Checkpoint : public BaseObject {
public:
    Checkpoint(const CF_V2& pos = CF_V2(0.0f, 0.0f)) noexcept: BaseObject(), position(pos) {}
    ~Checkpoint() noexcept override {}

    void Start() override;

    void OnCollisionEnter(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept override;

private:
    CF_V2 position;
};