#pragma once

#include <unordered_map>

namespace tramogi::input {

// This might not sit well with different keyboard layouts.
// The scancodes might also only refer to GLFW's scancodes, need to check.
// Also consider ASCII.
// TODO: Test on other keyboard layouts and think of another way if necessary.
enum class Key {
	A = 30,
	B = 48,
	C = 46,
	D = 32,
	E = 18,
	F = 33,
	G = 34,
	H = 35,
	I = 23,
	J = 36,
	K = 37,
	L = 38,
	M = 50,
	N = 49,
	O = 24,
	P = 25,
	Q = 16,
	R = 19,
	S = 31,
	T = 20,
	U = 22,
	V = 47,
	W = 17,
	X = 45,
	Y = 21,
	Z = 44,

	No0 = 2,
	No1 = 3,
	No2 = 4,
	No3 = 5,
	No4 = 6,
	No5 = 7,
	No6 = 8,
	No7 = 9,
	No8 = 10,
	No9 = 11,

	F1 = 59,
	F2 = 60,
	F3 = 61,
	F4 = 62,
	F5 = 63,
	F6 = 64,
	F7 = 65,
	F8 = 66,
	F9 = 67,
	F10 = 68,
	F11 = 87,
	F12 = 88,

	ArrowUp = 328,
	ArrowLeft = 331,
	ArrowDown = 336,
	ArrowRight = 333,

	Unknown,
};

class Keyboard {
public:
	bool is_pressed(Key key);
	void consume_key(Key key);

	void set_key(int scancode, bool is_pressed);

private:
	std::unordered_map<Key, bool> keys;
};

} // namespace tramogi::input
