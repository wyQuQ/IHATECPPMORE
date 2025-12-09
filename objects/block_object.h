#pragma once
#include "base_object.h"
#include <iostream>

//模版中两个参数分别代表方块的位置和贴图类型(1为草地，2为泥土）
template <CF_V2 position, int type>
class BlockObject : public BaseObject {
public:
    BlockObject() noexcept : BaseObject() {}
    ~BlockObject() noexcept {}

    void Start() override
    {
        // 设置精灵属性
        if (type == 1){
            SpriteSetStats("/sprites/block1.png", 1, 1, 0);
        }
        else if (type == 2){
            SpriteSetStats("/sprites/block2.png", 1, 1, 0);
        }
        else{
            std::cout << "Invalid block type: " << type << std::endl;
        }
        SetPosition(position);
        Scale(0.5f);
        SetVisible(true);
        SetPivot(0,0);
        SetDepth(0);
        
        // 设置碰撞箱
        SetupCollisionBox();
        
        // 设置为实体碰撞类型
        SetColliderType(ColliderType::SOLID);
    }
    
    //// 碰撞回调函数
    //void OnCollisionEnter(const ObjManager::ObjToken& other) override;
    //void OnCollisionStay(const ObjManager::ObjToken& other) override;
    //void OnCollisionExit(const ObjManager::ObjToken& other) override;

private:
    void SetupCollisionBox()
    {
        PngFrame frame = SpriteGetFrame();

        CF_Aabb aabb{};
        if (!frame.empty() && frame.w > 0 && frame.h > 0) {
            // 以贴图中心为原点，创建与精灵尺寸（考虑缩放）一致的 AABB
            float half_w = static_cast<float>(frame.w) * 0.5f ;
            float half_h = static_cast<float>(frame.h) * 0.5f ;
            aabb.min = cf_v2(-half_w, -half_h);
            aabb.max = cf_v2(half_w, half_h);

            
            // 设置碰撞形状
            SetShape(CF_ShapeWrapper::FromAabb(aabb));
            
            std::cout << "BlockObject collision box set: " 
                      << frame.w << "x" << frame.h << std::endl;
        } else {
            // 如果获取不到精灵尺寸，使用默认尺寸
            float half_w = 32.0f ;
            float half_h = 32.0f ;
            aabb.min = cf_v2(-half_w, -half_h);
            aabb.max = cf_v2(half_w, half_h);  // 默认64x64碰撞箱
            
            SetShape(CF_ShapeWrapper::FromAabb(aabb));
            std::cout << "BlockObject using default collision box: 64x64" << std::endl;
        }
    }
};