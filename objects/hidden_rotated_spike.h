#pragma once
#include "base_object.h"
#include "act_seq.h"

class HiddenRotatedSpike : public BaseObject
{
public:
    HiddenRotatedSpike(CF_V2 pos, int check_pos = 1, bool dir_left = true, int attack_count = 1, float move_time = 0.1f ) noexcept
        : BaseObject(), position(pos), check_count(check_pos), direction_left(dir_left), attack_count(attack_count), move_consumed_time(move_time) {}
    ~HiddenRotatedSpike() noexcept override {}

    // 生命周期
    void Start() override;

	// 每帧更新钩子
    void Update() override;

	// 碰撞回调
    void OnCollisionStay(const ObjManager::ObjToken& other_token, const CF_Manifold& manifold) noexcept override;

private:
    // 隐藏刺的位置
    CF_V2 position;

    // 指示刺的指向
    bool direction_left;

    // 指示攻击涵盖刺指向方向的几个格子
    int attack_count;

    // 控制刺的移动时间
    float move_consumed_time;

    // 用于指示检查范围涵盖刺指向方向的几个格子
    int check_count;

    // 动作序列钩子
    ActSeq m_act_seq;

	// 每个对象的动作仅允许触发一次
    bool once = true;
};