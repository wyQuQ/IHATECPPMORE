#include "base_object.h"
#include "base_physics.h" // 提供 CF_ShapeWrapper / v2math 等
#include <cmath>

void ManifoldDrawDebug(const CF_Manifold& m) noexcept {
    // 调试绘制碰撞信息：紫色线连接接触点，紫色箭头表示法向量
    cf_draw_push();
    cf_draw_push_color(CF_Color(1.0f, 0.5f, 1.0f, 1.0f)); // 粉紫色

    // 绘制接触点之间的连线（如果有两个接触点）
    if (m.count == 2) {
        CF_V2 cp0 = m.contact_points[0];
        cf_draw_circle2(cp0, 2.0f, 0.0f);
        CF_V2 cp1 = m.contact_points[1];
        cf_draw_circle2(cp1, 2.0f, 0.0f);
        cf_draw_line(cp0, cp1, 0.0f);
    }

    // 从每个接触点绘制法向量（短线表示方向）
    const float normal_length = 8.0f; // 法向量的可视长度
    for (int i = 0; i < m.count; ++i) {
        CF_V2 cp = m.contact_points[i];
        CF_V2 normal = m.n * normal_length;
        cf_draw_line(cp - normal, cp + normal, 0.0f);

        // 在法向量终点绘制一个小圆点，使方向更明显
        cf_draw_circle2(cp - normal, 2.0f, 0.0f);
        cf_draw_circle2(cp + normal, 2.0f, 0.0f);
    }

    cf_draw_pop_color();
    cf_draw_pop();
}

// 实现：根据 BaseObject 中的 CF_ShapeWrapper 绘制红色轮廓（noexcept）
void RenderBaseObjectCollisionDebug(const BaseObject* obj) noexcept
{
    if (!obj) return;

    const CF_ShapeWrapper& s = obj->GetShape();

    // 使用红色并保存绘制状态
    cf_draw_push();
    cf_draw_push_color(cf_color_red());

    switch (s.type)
    {
    case CF_SHAPE_TYPE_AABB:
    {
        const CF_Aabb& a = s.u.aabb;
        CF_V2 p1{ a.min.x, a.min.y };
        CF_V2 p2{ a.max.x, a.min.y };
        CF_V2 p3{ a.max.x, a.max.y };
        CF_V2 p4{ a.min.x, a.max.y };
        cf_draw_line(p1, p2, 0.0f);
        cf_draw_line(p2, p3, 0.0f);
        cf_draw_line(p3, p4, 0.0f);
        cf_draw_line(p4, p1, 0.0f);
        break;
    }
    case CF_SHAPE_TYPE_CIRCLE:
    {
        const CF_Circle& c = s.u.circle;
        // cf_draw_circle2(center, radius, angle)
        cf_draw_circle2(c.p, c.r, 0.0f);
        break;
    }
    case CF_SHAPE_TYPE_CAPSULE:
    {
        const CF_Capsule& cap = s.u.capsule;
        CF_V2 a = cap.a;
        CF_V2 b = cap.b;
        float r = cap.r;

        // 绘制中轴线
        cf_draw_line(a, b, 0.0f);

        // 绘制端点圆
        cf_draw_circle2(a, r, 0.0f);
        cf_draw_circle2(b, r, 0.0f);

        // 绘制连接矩形（用四条线近似）
        CF_V2 dir = CF_V2{ b.x - a.x, b.y - a.y };
        CF_V2 ndir = v2math::normalized(dir);
        CF_V2 perp = CF_V2{ -ndir.y * r, ndir.x * r }; // 垂直向量 * r

        CF_V2 p1{ a.x + perp.x, a.y + perp.y };
        CF_V2 p2{ b.x + perp.x, b.y + perp.y };
        CF_V2 p3{ b.x - perp.x, b.y - perp.y };
        CF_V2 p4{ a.x - perp.x, a.y - perp.y };

        cf_draw_line(p1, p2, 0.0f);
        cf_draw_line(p2, p3, 0.0f);
        cf_draw_line(p3, p4, 0.0f);
        cf_draw_line(p4, p1, 0.0f);
        break;
    }
    case CF_SHAPE_TYPE_POLY:
    {
        const CF_Poly& poly = s.u.poly;
        int n = poly.count;
        if (n > 1) {
            for (int i = 0; i < n; ++i) {
                CF_V2 p0 = poly.verts[i];
                CF_V2 p1 = poly.verts[(i + 1) % n];
                cf_draw_line(p0, p1, 0.0f);
            }
        }
        break;
    }
    default:
        // 未知类型：无动作
        break;
    }

    cf_draw_pop_color();
    cf_draw_pop();
}