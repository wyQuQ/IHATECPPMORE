#include "drawing_sequence.h"
#include "base_object.h"

#include <algorithm>
#include <vector>
#include <mutex>
#include <iostream>
#include <unordered_map>

// per-owner canvas 缓存（文件内静态）
// 说明：为每个注册的 BaseObject 缓存一个 CF_Canvas，用于在上传阶段存放像素数据并在渲染阶段复用。
// 该缓存通过 g_canvas_cache_mutex 保护；当对象注销时会释放对应的 canvas 资源。
namespace {
	struct CanvasCache {
		CF_Canvas canvas{};
		int w = 0;
		int h = 0;
	};
	static std::unordered_map<BaseObject*, CanvasCache> g_canvas_cache;
	static std::mutex g_canvas_cache_mutex;
}

// 单例访问：用户通过 DrawingSequence::Instance() 获取全局绘制序列管理器。
DrawingSequence& DrawingSequence::Instance() noexcept
{
	static DrawingSequence inst;
	return inst;
}

void DrawingSequence::Register(BaseObject* obj) noexcept
{
	if (!obj) return;
	std::lock_guard<std::mutex> lk(m_mutex);
	auto e = std::make_unique<Entry>();
	e->owner = obj;
	e->reg_index = m_next_reg_index++;
	m_entries.push_back(std::move(e));
}

void DrawingSequence::Unregister(BaseObject* obj) noexcept
{
	if (!obj) return;

	// 从全局缓存中销毁对应的 per-owner canvas（如果存在）。
	// 这是对象注销时释放显式图形资源的必要步骤。
	{
		std::lock_guard<std::mutex> lk(g_canvas_cache_mutex);
		auto it = g_canvas_cache.find(obj);
		if (it != g_canvas_cache.end()) {
			if (it->second.canvas.id != 0) {
				cf_destroy_canvas(it->second.canvas);
				it->second.canvas.id = 0;
			}
			g_canvas_cache.erase(it);
		}
	}

	// 从注册列表移除对象，保证后续 DrawAll/BlitAll 不再处理该对象。
	std::lock_guard<std::mutex> lk(m_mutex);
	m_entries.erase(std::remove_if(m_entries.begin(), m_entries.end(),
		[obj](const std::unique_ptr<Entry>& p) { return p->owner == obj; }), m_entries.end());
}

// DrawAll: 上传阶段（只做帧提取与像素上传，不执行绘制）
// 使用场景：应在主循环早期调用，以确保所有纹理在渲染阶段之前已被更新到 GPU。
void DrawingSequence::DrawAll()
{
	// 获取注册快照以减少持锁时间，保证高并发场景下的短锁粒度。
	std::vector<Entry> snapshot;
	{
		std::lock_guard<std::mutex> lk(m_mutex);
		snapshot.reserve(m_entries.size());
		for (auto& p : m_entries) snapshot.push_back(*p);
	}

	// 按 depth + 注册序号排序以保证确定性的绘制顺序。
	std::sort(snapshot.begin(), snapshot.end(), [](const Entry& a, const Entry& b) {
		int da = a.owner ? a.owner->GetDepth() : 0;
		int db = b.owner ? b.owner->GetDepth() : 0;
		if (da != db) return da < db;
		return a.reg_index < b.reg_index;
	});

	// 遍历快照：为每个可见对象提取当前帧并上传到其 per-owner canvas（仅上传）。
	for (auto& e : snapshot) {
		BaseObject* owner = e.owner;
		if (!owner) continue;

		// 跳过不可见对象以节省资源。
		if (!owner->IsVisible()) continue;

		PngFrame frame = owner->SpriteGetFrame();
		if (frame.empty()) continue;

		// 简单校验像素大小，防范无效数据导致的显存更新异常。
		size_t expected_bytes = static_cast<size_t>(frame.w) * static_cast<size_t>(frame.h) * 4u;
		if (frame.pixels.size() < expected_bytes) {
			std::cerr << "[DrawingSequence] pixel data size mismatch for owner\n";
			continue;
		}

		// 获取或创建 per-owner canvas（在 g_canvas_cache 上短锁）
		CF_Canvas local_canvas{};
		{
			std::lock_guard<std::mutex> lk(g_canvas_cache_mutex);
			auto it = g_canvas_cache.find(owner);
			if (it == g_canvas_cache.end()) {
				CanvasCache cc;
				cc.canvas = cf_make_canvas(cf_canvas_defaults(frame.w, frame.h));
				cc.w = frame.w;
				cc.h = frame.h;
				g_canvas_cache.emplace(owner, std::move(cc));
				local_canvas = g_canvas_cache[owner].canvas;
			} else {
				CanvasCache& cc = it->second;
				if (cc.canvas.id == 0 || cc.w != frame.w || cc.h != frame.h) {
					if (cc.canvas.id != 0) {
						cf_destroy_canvas(cc.canvas);
						cc.canvas.id = 0;
					}
					cc.canvas = cf_make_canvas(cf_canvas_defaults(frame.w, frame.h));
					cc.w = frame.w;
					cc.h = frame.h;
				}
				local_canvas = cc.canvas;
			}
		}

		if (local_canvas.id == 0) {
			std::cerr << "[DrawingSequence] failed to create canvas for owner\n";
			continue;
		}

		// 上传像素到 canvas 的目标纹理（应在主线程安全调用）。
		CF_Texture tgt = cf_canvas_get_target(local_canvas);
		size_t bytes = static_cast<size_t>(frame.w) * static_cast<size_t>(frame.h) * sizeof(CF_Pixel);
		cf_texture_update(tgt, frame.pixels.data(), bytes);
	}
}

// BlitAll: 渲染阶段（将已上传的 canvas 绘制到屏幕）
// 说明：该阶段读取对象的变换信息并执行统一的绘制逻辑。锁策略与回调一致，绘制主体被 try/catch 包围以保证稳定性。
void DrawingSequence::BlitAll()
{
	// 获取注册快照以减少持锁时间。
	std::vector<Entry> snapshot;
	{
		std::lock_guard<std::mutex> lk(m_mutex);
		snapshot.reserve(m_entries.size());
		for (auto& p : m_entries) snapshot.push_back(*p);
	}

	std::sort(snapshot.begin(), snapshot.end(), [](const Entry& a, const Entry& b) {
		int da = a.owner ? a.owner->GetDepth() : 0;
		int db = b.owner ? b.owner->GetDepth() : 0;
		if (da != db) return da < db;
		return a.reg_index < b.reg_index;
	});

	for (auto& e : snapshot) {
		BaseObject* owner = e.owner;
		if (!owner) continue;

		// 不可见对象跳过绘制。
		if (!owner->IsVisible()) continue;

		// 查找缓存中的 canvas（短锁粒度）
		CF_Canvas local_canvas{};
		int w = 0, h = 0;
		{
			std::lock_guard<std::mutex> lk(g_canvas_cache_mutex);
			auto it = g_canvas_cache.find(owner);
			if (it == g_canvas_cache.end()) {
				// 未上传过的 canvas：跳过该对象的绘制
				continue;
			}
			local_canvas = it->second.canvas;
			w = it->second.w;
			h = it->second.h;
		}

		if (local_canvas.id == 0 || w <= 0 || h <= 0) continue;

		// 在读取与绘制相关的共享状态时加锁，绘制主体放入 try/catch 以记录并忽略异常，保证主循环稳定运行。
		std::lock_guard<std::mutex> draw_lk(m_mutex);
		try {
			// 默认绘制约定：
			//  - 翻转总是围绕贴图中心执行
			//  - 旋转围绕用户提供的枢轴（pivot），pivot 为相对于贴图中心的像素偏移
			//  - scale（用户接口）应影响贴图显示大小，围绕 pivot 执行缩放
			CF_V2 pos = owner ? owner->GetPosition() : cf_v2(0.0f, 0.0f);
			const float fw = static_cast<float>(w);
			const float fh = static_cast<float>(h);
			const float cx = fw * 0.5f;
			const float cy = fh * 0.5f;

			// 从 BaseObject 读取旋转、翻转标志、缩放与枢轴点（枢轴以"相对于贴图中心"的像素偏移表示）
			const float rot = owner ? owner->GetRotation() : 0.0f;
			const bool flipx = owner ? owner->SpriteGetFlipX() : false;
			const bool flipy = owner ? owner->SpriteGetFlipY() : false;
			const float user_sx = owner ? owner->GetScaleX() : 1.0f;
			const float user_sy = owner ? owner->GetScaleY() : 1.0f;
			CF_V2 pivot = owner ? owner->GetPivot() : cf_v2(0.0f, 0.0f);

			// 绘制变换步骤（语义清晰且高效）：
			// 1) 移动枢轴到世界位置 pos（表示枢轴的世界坐标）
			// 2) 若需要翻转/缩放：围绕枢轴执行平移->scale->回平移（缩放会同时影响贴图大小与枢轴的映射）
			// 3) 若需要旋转：在枢轴处旋转（旋转符号受翻转影响）
			// 4) 将贴图左上角绘制到 (-pivot) 处，使枢轴映射到世界 pos
			cf_draw_push();

			// 将枢轴移动到世界坐标 pos
			cf_draw_translate(pos.x, pos.y);

			// 围绕枢轴缩放（与翻转独立叠加）
			if (user_sx != 1.0f || user_sy != 1.0f) {
				cf_draw_scale(user_sx, user_sy);
			}

			// 翻转（作为 scale 的特殊情况），先按翻转在枢轴处做变换
			const float flip_sx = flipx ? -1.0f : 1.0f;
			const float flip_sy = flipy ? -1.0f : 1.0f;
			if (flipx || flipy) {
				cf_draw_translate(- pivot.x, - pivot.y);
				cf_draw_scale(flip_sx, flip_sy);
				cf_draw_translate(pivot.x, pivot.y);
			}

			// 旋转：在枢轴处旋转（旋转方向需要考虑翻转对符号的影响）
			if (rot != 0.0f) {
				cf_draw_translate(-2 * pivot.x * flipx, -2 * pivot.y * flipy);
				cf_draw_rotate(-rot * flip_sx * flip_sy);
				cf_draw_translate(2 * pivot.x * flipx, 2 * pivot.y * flipy);
			}

			// 最终绘制：cf_draw_canvas 会受到之前的缩放/旋转/平移影响，从而实现按 scale 缩放贴图显示大小
			cf_draw_canvas(local_canvas, -pivot, cf_v2(fw, fh));

			cf_draw_pop();
		} catch (const std::exception& ex) {
			// 记录异常但不抛出，保证绘制循环继续执行
			std::cerr << "[DrawingSequence] draw threw std::exception: " << ex.what() << "\n";
		} catch (...) {
			std::cerr << "[DrawingSequence] draw threw unknown exception\n";
		}
	}
}