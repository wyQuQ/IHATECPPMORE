#include "room_loader.h"
#include "input.h"

class EmptyRoom : public BaseRoom {
public:
	EmptyRoom() noexcept {}
	~EmptyRoom() noexcept override {}

	void RoomLoad() override {
		OUTPUT({ "EmptyRoom" }, "RoomLoad called.");
	}
	void RoomUpdate() override {
		if (Input::IsKeyInState(CF_KEY_P, KeyState::Down)) {
			RoomLoader::Instance().Load("FirstRoom");
		}
	}
	void RoomUnload() override {
		OUTPUT({ "TestRoom" }, "RoomUnload called.");
	}
};

REGISTER_ROOM("EmptyRoom", EmptyRoom);