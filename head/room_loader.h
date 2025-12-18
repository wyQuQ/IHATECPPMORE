#pragma once
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <cstddef>
#include "debug_config.h"
#include "delegate.h"
#include "obj_manager.h"

extern Delegate<> main_thread_on_update;

class BaseRoom {
public:
	BaseRoom() noexcept {}
	virtual ~BaseRoom() noexcept {}

	virtual void RoomLoad() {}
	virtual void RoomUpdate() {}
	virtual void RoomUnload() {}

	void LoadRoom() {
		// 调用房间加载逻辑
		RoomLoad();
	}

	void UnloadRoom() {
		// 调用房间卸载逻辑
		RoomUnload();
		// 由控制器销毁所有对象
		ObjManager::Instance().DestroyAll();
		// 清理主线程更新委托
		main_thread_on_update.clear();
	}
};

class RoomLoader {
public:
	RoomLoader() noexcept {}
	~RoomLoader() noexcept {}

	static RoomLoader& Instance() noexcept {
		static RoomLoader instance;
		return instance;
	}

	// 通过房间引用加载房间
	void Load(const BaseRoom& room) {
		if (current_room_) {
			current_room_->get().UnloadRoom();
		}
		current_room_ = std::ref(const_cast<BaseRoom&>(room));
		current_room_->get().LoadRoom();
	}

	// 通过房间名称加载房间
	void Load(const std::string& room_name) {
		if (room_name.empty()) {
			OUTPUT({ "RoomLoader::Load" }, "Attempted to load room with empty name.");
			return;
		}

		auto it = rooms_.find(room_name);
		if (it == rooms_.end() || !it->second) {
			OUTPUT({ "RoomLoader::Load" }, "Room not found or null:", room_name);
			return;
		}

		Load(*it->second);
		OUTPUT({ "RoomLoader::Load" }, "Loaded room:", room_name);
	}

	// 加载初始房间
	void LoadInitial() {
		if (initial_room_) {
			Load(initial_room_->get());
			return;
		}

		if (!rooms_.empty() && rooms_.begin()->second) {
			Load(*rooms_.begin()->second);
			return;
		}

		OUTPUT({ "RoomLoader::LoadInitial" }, "No rooms registered to load initial room.");
	}

	// 更新当前房间
	void UpdateCurrent() {
		if (current_room_) {
			current_room_->get().RoomUpdate();
		}
		else OUTPUT({ "RoomLoader::UpdateCurrent" }, "No current room to update.");
	}

	// 卸载当前房间
	void UnloadCurrent() {
		if (!current_room_) {
			OUTPUT({ "RoomLoader::UnloadCurrent" }, "No current room to unload.");
			return;
		}
		current_room_->get().UnloadRoom();
		current_room_.reset();
	}

	// 注册房间，若名字重复则覆盖，可选标记为初始房间
	void RegisterRoom(const std::string& room_name, std::unique_ptr<BaseRoom> room, bool initial = false) {
		if (room_name.empty() || !room) {
			OUTPUT({ "RoomLoader::RegisterRoom" }, "Attempted to register room with empty name or null room pointer.");
			return;
		}

		auto [it, inserted] = rooms_.insert_or_assign(room_name, std::move(room));
		if (!it->second) {
			OUTPUT({ "RoomLoader::RegisterRoom" }, "Failed to register room:", room_name);
			return;
		}

		if (initial) {
			initial_room_ = std::ref(*it->second);
			OUTPUT({ "RoomLoader::RegisterRoom" }, "Registered initial room:", room_name);
		}
	}

	const BaseRoom* GetCurrentRoom() const noexcept {
		return current_room_ ? &current_room_->get() : nullptr;
	}

	const BaseRoom* GetInitialRoom() const noexcept {
		return initial_room_ ? &initial_room_->get() : nullptr;
	}

	size_t GetEstimatedMemoryUsageBytes() const noexcept {
		size_t total = 0;
		total += rooms_.bucket_count() * sizeof(decltype(rooms_)::value_type);
		total += rooms_.size() * sizeof(std::pair<const std::string, std::unique_ptr<BaseRoom>>);
		return total;
	}

 private:
	// 已注册的房间映射
	std::unordered_map<std::string, std::unique_ptr<BaseRoom>> rooms_;
	// 当前加载的房间
	std::optional<std::reference_wrapper<BaseRoom>> current_room_;
	// 初始房间
	std::optional<std::reference_wrapper<BaseRoom>> initial_room_;
};

namespace room_loader_detail {

	// 隐藏在实现细节空间中的注册辅助模板，用于避免将注册逻辑暴露给公共接口。
	// 该模板配合 REGISTER_* 宏在编译期运行，通过静态变量在模块初始化时立即完成房间注册，从而将具体的房间派生类与 main 函数及其加载流程解耦。
	template<typename RoomType>
	class RoomRegistrar {
	public:
		// 构造期间立即创建指定房间并委托给 RoomLoader，initial 标记决定是否成为默认起始房间。
		explicit RoomRegistrar(std::string_view room_name, bool initial = false) {
			static_assert(std::is_base_of_v<BaseRoom, RoomType>, "RoomRegistrar requires BaseRoom derivation.");
			RoomLoader::Instance().RegisterRoom(std::string(room_name), std::make_unique<RoomType>(), initial);
		}
	};

} // namespace room_loader_detail

#define ROOM_LOADER_CONCAT_IMPL(x, y) x##y
#define ROOM_LOADER_CONCAT(x, y) ROOM_LOADER_CONCAT_IMPL(x, y)

// 使用静态注册器实现模块加载时自动调用 RoomLoader::RegisterRoom 以完成房间注册。
// 宏展开会生成一个 const static 的 RoomRegistrar 实例，利用 __COUNTER__ 生成独一无二的变量名，阻止命名冲突。
// 注册器构造过程中负责创建实际房间对象并将其传递给 RoomLoader，初始房间通过额外参数标记。
// 这一套机制在编译期即完成填写，实现了将具体房间派生类与 main 函数加载逻辑的彻底解耦。
// 若要将房间设置为初始房间，请使用REGISTER_INITIAL_ROOM
#define REGISTER_ROOM(NAME, TYPE) \
	static const room_loader_detail::RoomRegistrar<TYPE> ROOM_LOADER_CONCAT(room_registrar_, __COUNTER__)(NAME, false)

// 注册房间并标记为初始
#define REGISTER_INITIAL_ROOM(NAME, TYPE) \
	static const room_loader_detail::RoomRegistrar<TYPE> ROOM_LOADER_CONCAT(room_registrar_, __COUNTER__)(NAME, true)