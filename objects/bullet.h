#pragma once
#include "base_object.h"

class Bullet : public BaseObject {
public:
    Bullet() noexcept : BaseObject(), m_spawn_frame(0) {}
    ~Bullet() noexcept {}

    void Start() override;
    void Update() override;

    // 碰撞回调：用于检测与 任意物体 的接触
    void OnCollisionStay(const ObjManager::ObjToken& other, const CF_Manifold& manifold) noexcept override;

private:
    int m_spawn_frame;
    // 以帧为单位的生存期：5 秒 * 50 FPS = 250 帧（可按需调整）
    static int kLifetimeFrames;
};