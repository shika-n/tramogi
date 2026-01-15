#pragma once

#include <unordered_map>

namespace tramogi::input {

enum class MouseButton {
	Left,
	Right,
	Middle,

	Unknown,
};

class Mouse {
public:
	bool is_pressed(MouseButton key);
	void consume_mouse(MouseButton key);

	void set_mouse_button(int button, bool is_pressed);
	void set_mouse_position(double x, double y);

private:
	std::unordered_map<MouseButton, bool> buttons;
	double x = 0.0;
	double y = 0.0;
};

} // namespace tramogi::input
