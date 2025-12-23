#include "tramogi/input/keyboard.h"

namespace tramogi::input {

bool Keyboard::is_pressed(Key key) {
	if (!keys.contains(key)) {
		return false;
	}
	return keys[key];
}

void Keyboard::consume_key(Key key) {
	if (!keys.contains(key)) {
		return;
	}
	keys[key] = false;
}

void Keyboard::set_key(int scancode, bool is_pressed) {
	keys[static_cast<Key>(scancode)] = is_pressed;
}

} // namespace tramogi::input
