#include "tramogi/core/input.h"
#include <cstdint>

namespace tramogi::core {

bool Input::is_pressed(uint32_t key) {
	if (key < 'A') {
		DLOG("{} is smalled than 'A'", key);
		return false;
	}

	uint32_t key_index = key - 'A';
	if (key_index >= keys.size()) {
		DLOG("{} exceeds keys size", key);
		return false;
	}

	return keys[key_index];
}

void Input::consume_key(uint32_t key) {
	if (key < 'A') {
		DLOG("{} is smalled than 'A'", key);
		return;
	}

	uint32_t key_index = key - 'A';
	if (key_index >= keys.size()) {
		DLOG("{} exceeds keys size", key);
		return;
	}

	keys[key_index] = false;
}

void Input::set_key(uint32_t key, bool is_pressed) {
	DLOG("set_key is called!");
	if (key < 'A') {
		DLOG("{} is smalled than 'A'", key);
		return;
	}

	uint32_t key_index = key - 'A';
	if (key_index >= keys.size()) {
		DLOG("{} exceeds keys size", key);
		return;
	}

	keys[key_index] = is_pressed;
}

} // namespace tramogi::core
