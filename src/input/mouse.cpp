#include "tramogi/input/mouse.h"

namespace tramogi::input {

bool Mouse::is_pressed(MouseButton key) {
	if (!buttons.contains(key)) {
		return false;
	}
	return buttons[key];
}

void Mouse::consume_mouse(MouseButton key) {
	if (!buttons.contains(key)) {
		return;
	}
	buttons[key] = false;
}

void Mouse::set_mouse_button(int button, bool is_pressed) {
	buttons[static_cast<MouseButton>(button)] = is_pressed;
}

void Mouse::set_mouse_position(double x, double y) {
	this->x = x;
	this->y = y;
}

} // namespace tramogi::input
