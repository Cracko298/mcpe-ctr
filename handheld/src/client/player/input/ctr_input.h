#ifndef NET_MINECRAFT_CLIENT_PLAYER__N3dsInput_H__
#define NET_MINECRAFT_CLIENT_PLAYER__N3dsInput_H__

#include "KeyboardInput.h"
#include "ITurnInput.h"
#include "touchscreen/TouchInputHolder.h"
#include "MouseBuildInput.h"
#include "../../../platform/input/Controller.h"

#ifdef __3DS__
#include <3ds.h>
#endif

// На 3DS: 1 - левый стик (Circle Pad), 2 - правый стик (C-Stick на New 3DS или CPP)
static const int moveStick = 1;
static const int lookStick = 2;

class N3dsTurnBuild : public UnifiedTurnBuild {
	Minecraft* _mc;
public:
	N3dsTurnBuild(int turnMode, int width, int height, float maxMovementDelta, float sensitivity, IInputHolder* holder, Minecraft* minecraft) :
		UnifiedTurnBuild(turnMode, width, height, maxMovementDelta, sensitivity, holder, minecraft), _mc(minecraft) {}

	TurnDelta getTurnDelta() override {
		// Если игрок водит стилусом/пальцем по нижнему экрану (камера или UI)
		if(Multitouch::getFirstActivePointerIdEx() >= 0) {
			return UnifiedTurnBuild::getTurnDelta();
		}

		TurnDelta td = UnifiedTurnBuild::getTurnDelta();

		// Считываем C-Stick. 
		// Мертвая зона (0.2f) важна, так как C-Stick часто "дрифтит"
		float stickX = Controller::getTransformedX(lookStick, 0.2f, 1.25f, true);
		float stickY = Controller::getTransformedY(lookStick, 0.2f, 1.25f, true);

		float dx = 0, dy = 0;
		float dt = getDeltaTime();
		
		// Чувствительность для C-Stick. Поднял т.к. пимпочка тугая
		// и крутить камеру было невозможно.
		const float MaxTurnX = 450.0f;
		const float MaxTurnY = 360.0f;
		
		dx = linearTransform( stickX, 0.2f, MaxTurnX ) * dt;
		dy = linearTransform( stickY, 0.2f, MaxTurnY ) * dt;
		
		td.x += dx;
		td.y += dy;
		return td;
	}

	bool tickBuild(Player* p, BuildActionIntention* bai) override {
		// Тапаем по экрану
		if(Multitouch::getFirstActivePointerIdEx() >= 0) {
			return UnifiedTurnBuild::tickBuild(p, bai);
		}

		bool isPhysicalButton = true;
#ifdef __3DS__
		if (hidKeysHeld() & KEY_TOUCH) {
			isPhysicalButton = false;
		}
#endif

		// Левая кнопка (обычно замаплена на ZR или R в Controller.cpp) - ломать блоки/атака
		if (Mouse::getButtonState(MouseAction::ACTION_LEFT) != 0) {
			bool validAction = isPhysicalButton;
			if (!isPhysicalButton) {
				float mx = Mouse::getX();
				float my = Mouse::getY();
				float s = Gui::GuiScale;

				if (_mc->options.xybaCamera) {
					// XYBA mode: Enforce explicit hit-testing for the camera zone.
					int screenWidth = _mc->gui.getBottomGuiWidth();
					int mapX0 = screenWidth - 54;
					float camLeft = 4 * s;
					float camRight = mapX0 * s;
					float camTop = 31 * s;
					float camBottom = 236 * s;

					if (mx >= camLeft && mx <= camRight && my >= camTop && my <= camBottom) {
						int bx0, by0, bx1, by1;
						_mc->gui.getControlButtonRect(1, bx0, by0, bx1, by1);
						if (mx >= (bx0 - 4) * s && mx <= (bx1 + 4) * s && my >= (by0 - 4) * s && my <= (by1 + 4) * s) {
							validAction = false;
						} else {
							_mc->gui.getControlButtonRect(2, bx0, by0, bx1, by1);
							if (mx >= (bx0 - 4) * s && mx <= (bx1 + 4) * s && my >= (by0 - 4) * s && my <= (by1 + 4) * s) {
								validAction = false;
							} else {
								validAction = true;
							}
						}
					} else {
						validAction = false; // Outside camera zone (minimap, inventory button, etc)
					}
				} else {
					// Cam Zone mode: Taps to place/break blocks are verified here.
					validAction = isInsideArea(mx, my);
				}
			}

			if (validAction) {
				if(totalMineTicks++ <= 0) {
					*bai = BuildActionIntention(BuildActionIntention::BAI_FIRSTREMOVE | BuildActionIntention::BAI_ATTACK);
					return true;
				}
				else {
					*bai = BuildActionIntention(BuildActionIntention::BAI_REMOVE | BuildActionIntention::BAI_ATTACK);
					return true;
				}
			}
		} else {
			totalMineTicks = 0;
		}

		// Правая кнопка (обычно ZL или L) - ставить блоки
		if (Mouse::getButtonState(MouseAction::ACTION_RIGHT) != 0) {
			bool validAction = isPhysicalButton;
			if (!isPhysicalButton) {
				float mx = Mouse::getX();
				float my = Mouse::getY();
				float s = Gui::GuiScale;

				if (_mc->options.xybaCamera) {
					int screenWidth = _mc->gui.getBottomGuiWidth();
					int mapX0 = screenWidth - 54;
					float camLeft = 4 * s;
					float camRight = mapX0 * s;
					float camTop = 31 * s;
					float camBottom = 236 * s;

					if (mx >= camLeft && mx <= camRight && my >= camTop && my <= camBottom) {
						int bx0, by0, bx1, by1;
						_mc->gui.getControlButtonRect(1, bx0, by0, bx1, by1);
						if (mx >= (bx0 - 4) * s && mx <= (bx1 + 4) * s && my >= (by0 - 4) * s && my <= (by1 + 4) * s) {
							validAction = false;
						} else {
							_mc->gui.getControlButtonRect(2, bx0, by0, bx1, by1);
							if (mx >= (bx0 - 4) * s && mx <= (bx1 + 4) * s && my >= (by0 - 4) * s && my <= (by1 + 4) * s) {
								validAction = false;
							} else {
								validAction = true;
							}
						}
					} else {
						validAction = false;
					}
				} else {
					validAction = isInsideArea(mx, my);
				}
			}

			if (validAction) {
				if ((buildHoldTicks++ % buildDelayTicks) == 0) {
					*bai = BuildActionIntention(BuildActionIntention::BAI_BUILD | BuildActionIntention::BAI_INTERACT);
					return true;
				}
			}
 		} else {
			buildHoldTicks = 0;
		}

		return false;
	}

	void onConfigChanged(const Config& c) override {
		UnifiedTurnBuild::onConfigChanged(c);
	}

private:
	int totalMineTicks = 0;
	int buildHoldTicks = 0;
	int buildDelayTicks = 5;
};

class N3dsMoveInput : public KeyboardInput {
	typedef KeyboardInput super;
public:
	N3dsMoveInput(Options* options)
	:	super(options)
	{
		sprintTapTime = 0;
		sprintForwardHeld = false;
		stickSprinting = false;
		sneakToggleState = false;
		wasSneakDown = false;
	}

	void tick(Player* player) override {
		super::tick(player);
		
		bool isSneakDown = keys[KEY_SNEAK];
		if (isSneakDown && !wasSneakDown) {
			sneakToggleState = !sneakToggleState;
		}
		wasSneakDown = isSneakDown;

		sneaking = sneakToggleState;
		wantDown = sneaking;

		// Левый Circle Pad
		float stickX = Controller::getTransformedX(moveStick, 0.2f, 1.25f, true);
		float stickY = Controller::getTransformedY(moveStick, 0.2f, 1.25f, true);
		if (sneaking) {
			stickX *= 0.3f;
			stickY *= 0.3f;
		}
		xa += -stickX;
		ya += -stickY;
		updateStickSprint(-stickY);
		sprinting = stickSprinting;
	}

	void releaseAllKeys() override {
		super::releaseAllKeys();
		sprintTapTime = 0;
		sprintForwardHeld = false;
		stickSprinting = false;
		wasSneakDown = false;
		sneakToggleState = false;
	}

private:
	void updateStickSprint(float forward) {
		if (sprintTapTime > 0) sprintTapTime--;

		const bool forwardNow = forward > 0.82f;
		const bool released = forward < 0.35f;

		if (!forwardNow) {
			if (released) {
				sprintForwardHeld = false;
				stickSprinting = false;
			}
			return;
		}

		if (!sprintForwardHeld) {
			if (sprintTapTime > 0) stickSprinting = true;
			else sprintTapTime = 7;
			sprintForwardHeld = true;
		}
	}

	int sprintTapTime;
	bool sprintForwardHeld;
	bool stickSprinting;
	bool sneakToggleState;
	bool wasSneakDown;
};

class N3dsInputHolder : public IInputHolder {
	static const int MovementLimit = 200; // per update

public:
	N3dsInputHolder(Minecraft* mc, Options* options) :
		_mc(mc),
		_move(options),
		_turnBuild(UnifiedTurnBuild::MODE_DELTA, mc->width, mc->height, (float)MovementLimit, 1, this, mc),
		_minimapArea(0,0,0,0),
		_btn1Area(0,0,0,0),
		_btn2Area(0,0,0,0),
		_camZoneExcludeArea1(0,0,0,0),
		_camZoneExcludeArea2(0,0,0,0),
		_camZoneExcludeArea3(0,0,0,0),
		_camZoneExcludeArea4(0,0,0,0),
		_screenArea(0,0,0,0)
	{
		onConfigChanged(createConfig(mc));
	}
	~N3dsInputHolder() = default;

	void onConfigChanged(const Config& c) override {
		_move.onConfigChanged(c);
		_turnBuild.moveArea = RectangleArea(0,0,0,0);
		_turnBuild.inventoryArea = _mc->gui.getRectangleArea( _mc->options.isLeftHanded ? 1 : -1 );
		// Чувствительность тачскрина (стилус по нижнему экрану)
		_turnBuild.setSensitivity(c.options->isJoyTouchArea ? 2.8f : 1.8f);
		((ITurnInput*)&_turnBuild)->onConfigChanged(c);

		// In Cam Zone mode and XYBA mode, restrict the touch area to only the Cam Zone square
		int screenWidth  = _mc->gui.getBottomGuiWidth();
		int screenHeight = _mc->gui.getBottomGuiHeight();
		int mapX0 = screenWidth - 54;
		int py0 = 31;
		int px0 = 4;
		int py1 = screenHeight - 4;
		float s = Gui::GuiScale; // Scale from GUI to physical pixels

		// Use the actual physical screen dimensions (320x240) to prevent 
		// small crevices caused by floating point / integer scaling mismatches.
		_camZoneExcludeArea1 = RectangleArea(0, 0, px0 * s, 240); // Left
		_camZoneExcludeArea2 = RectangleArea(mapX0 * s, 0, 320, 240); // Right
		_camZoneExcludeArea3 = RectangleArea(0, 0, 320, py0 * s); // Top
		_camZoneExcludeArea4 = RectangleArea(0, py1 * s, 320, 240); // Bottom

		_screenArea = RectangleArea(0, 0, 320, 240);
		
		// Reset the state entirely to overwrite any inherited behaviors from UnifiedTurnBuild
		_turnBuild.setIncludeArea(&_screenArea);
		_turnBuild.addExcludeArea(&_camZoneExcludeArea1);
		_turnBuild.addExcludeArea(&_camZoneExcludeArea2);
		_turnBuild.addExcludeArea(&_camZoneExcludeArea3);
		_turnBuild.addExcludeArea(&_camZoneExcludeArea4);

		if (c.options->xybaCamera) {
			// XYBA mode: Also explicitly exclude the Jump and Inventory buttons.
			int bx0, by0, bx1, by1;
			_mc->gui.getControlButtonRect(1, bx0, by0, bx1, by1);
			_btn1Area = RectangleArea((bx0 - 4) * s, (by0 - 4) * s, (bx1 + 4) * s, (by1 + 4) * s);
			_turnBuild.addExcludeArea(&_btn1Area);

			_mc->gui.getControlButtonRect(2, bx0, by0, bx1, by1);
			_btn2Area = RectangleArea((bx0 - 4) * s, (by0 - 4) * s, (bx1 + 4) * s, (by1 + 4) * s);
			_turnBuild.addExcludeArea(&_btn2Area);
		} else {
			// Cam Zone mode: Only exclude the inventory button
			_turnBuild.addExcludeArea(&_turnBuild.inventoryArea);
		}
	}

	bool allowPicking() override {
		if (_mc->options.isJoyTouchArea || _mc->options.xybaCamera) {
			// Crosshair mode (Split Controls ON) or XYBA Camera Mode.
			// Picking MUST ALWAYS happen at the center of the top screen (crosshair),
			// regardless of touch input or location.
			mousex = _mc->width / 2;
			mousey = _mc->height / 2;
			return true;
		}

		// Split Controls OFF (Cam Zone mode):
		// Picking happens where the user is touching on the bottom screen, 
		// but ONLY if the touch is inside the valid camera zone square.
		int pointer = Multitouch::getFirstActivePointerIdEx();

		if(pointer >= 0) { 
			const float x = Multitouch::getX(pointer);
			const float y = Multitouch::getY(pointer);

			if (_turnBuild.isInsideArea(x, y)) {
				mousex = x;
				mousey = y;
				return true;
			}
			else {
				return false;
			}
		}
		else {
			// Если тача нет, прицел в центре верхнего экрана
			mousex = _mc->width / 2;
			mousey = _mc->height / 2;
			return true;
		}
	}

	void render(float alpha) override {
		_turnBuild.render(alpha);
	}

	IMoveInput*		getMoveInput()  override { return &_move; }
	ITurnInput*		getTurnInput()  override { return &_turnBuild; }
	IBuildInput*	getBuildInput() override { return &_turnBuild; }

private:
	N3dsMoveInput _move;
	N3dsTurnBuild _turnBuild;
	Minecraft* _mc;
	RectangleArea _minimapArea;
	RectangleArea _btn1Area;
	RectangleArea _btn2Area;
	RectangleArea _camZoneExcludeArea1;
	RectangleArea _camZoneExcludeArea2;
	RectangleArea _camZoneExcludeArea3;
	RectangleArea _camZoneExcludeArea4;
	RectangleArea _screenArea;
};

#endif /*NET_MINECRAFT_CLIENT_PLAYER__N3dsInput_H__*/
