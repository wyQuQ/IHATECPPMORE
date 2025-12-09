#include "backgroud.h"

void Backgroud::Start()
{
	SpriteSetStats("/sprites/background.png", 1, 1, -1000);
	SetPosition(cf_v2(0.0f, 0.0f));
	Scale(1.0f);
	SetVisible(true);
	SetPivot(0, 0);
}




