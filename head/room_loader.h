#pragma once
#include <functional>
#include <string>
#include <unordered_map>
#include "debug_config.h"

class Room {
public:
	Room() noexcept {}
	~Room() noexcept {}
	void SetInitial(std::function<void()> ini) {
		initial_func = ini;
	}
	void LoadRoom() {
		if (initial_func) {
			initial_func();
		}
		else {
			OUTPUT({ "Room" }, "No initial function set for Room.");
		}
	}
private:
	std::function<void()> initial_func;
};

class RoomLoader {
public:
	RoomLoader() noexcept {}
	~RoomLoader() noexcept {}
	void Load(const std::string& room_name) {
		auto it = room_map.find(room_name);
		if (it != room_map.end()) {
			OUTPUT({ "RoomLoader" }, "Loading room: ", room_name);
			it->second.LoadRoom();
		}
		else {
			OUTPUT({ "RoomLoader" }, "Room not found: ", room_name);
		}
	}
	void RegisterRoom(const std::string& room_name, const Room& room) {
		room_map[room_name] = room;
	}
private:
	std::unordered_map<std::string, Room> room_map;
};