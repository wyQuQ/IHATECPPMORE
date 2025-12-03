#include "base_object.h"
#include "drawing_sequence.h" // 在 cpp 中包含，获得 DrawingSequence 的完整定义
#include <iostream>

// 在实现文件中包含 InstanceController 以避免头文件循环依赖
#include "obj_manager.h"

void BaseObject::SpriteSetSource(const std::string& path, int count, bool set_shape_aabb) noexcept
{
    std::string current_path = "";
    bool had = HasPath(&current_path);
    if (had && current_path == path)return;
    if (had) {
        DrawingSequence::Instance().Unregister(this);
    }
    PngSprite::ClearPath();
    PngSprite::SetPath(path);
    SpriteSetVerticalFrameCount(count);
    bool has = HasPath();
    if (has) {
        DrawingSequence::Instance().Register(this);
        if (set_shape_aabb) {
            PngFrame frame = SpriteGetFrame();
            if (frame.w > 0 && frame.h > 0) {
                SetCenteredAabb(static_cast<float>(frame.w) * 0.5f, static_cast<float>(frame.h) * 0.5f);
            }
        }
    }
}

void BaseObject::SpriteClearPath() noexcept
{
    bool had = HasPath();
    PngSprite::ClearPath();
    if (had) {
        DrawingSequence::Instance().Unregister(this);
    }
}

void BaseObject::TweakColliderWithPivot(const CF_V2& pivot) noexcept
{
    CF_ShapeWrapper shape = get_shape();
    switch (shape.type) {
    case CF_ShapeType::CF_SHAPE_TYPE_AABB:
        shape.u.aabb.min -= pivot;
        shape.u.aabb.max -= pivot;
		break;
    case CF_ShapeType::CF_SHAPE_TYPE_CIRCLE:
		shape.u.circle.p -= pivot;
        break;
	case CF_ShapeType::CF_SHAPE_TYPE_CAPSULE:
		shape.u.capsule.a -= pivot;
        shape.u.capsule.b -= pivot;
		break;
	case CF_ShapeType::CF_SHAPE_TYPE_POLY:
        for (int i = 0; i < shape.u.poly.count; ++i) {
            shape.u.poly.verts[i] -= pivot;
		}
        break;
	default:
		// 不支持的形状类型
		std::cerr << "[BaseObject] TweakColliderWithPivot: Unsupported shape type " << static_cast<int>(shape.type) << std::endl;
		break;
    }
	set_shape(shape);
}

BaseObject::~BaseObject() noexcept
{
    OnDestroy();
    DrawingSequence::Instance().Unregister(this);
}