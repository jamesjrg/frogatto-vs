#include "SDL.h"

#include <iostream>
#include <vector>

#include "foreach.hpp"
#include "joystick.hpp"
#include "preferences.hpp"

namespace joystick {

namespace {
std::vector<SDL_Joystick*> joysticks;

const int threshold = 32700;
}

manager::manager() {
	for(int n = 0; n != SDL_NumJoysticks(); ++n) {
		SDL_Joystick* j = SDL_JoystickOpen(n);
		if(j) {
			joysticks.push_back(j);
		}
	}

	std::cerr << "initialized " << joysticks.size() << " joysticks\n";
}

manager::~manager() {
	foreach(SDL_Joystick* j, joysticks) {
		SDL_JoystickClose(j);
	}
	joysticks.clear();
}

void update() {
	if(preferences::use_joystick()) {
		SDL_JoystickUpdate();
	}
}

bool up() {
	if(!preferences::use_joystick()) {
		return false;
	}

	foreach(SDL_Joystick* j, joysticks) {
		const int nhats = SDL_JoystickNumHats(j);
		for(int n = 0; n != nhats; ++n) {
			const Uint8 state = SDL_JoystickGetHat(j, n);
			switch(state) {
			case SDL_HAT_UP:
			case SDL_HAT_RIGHTUP:
			case SDL_HAT_LEFTUP:
					return true;
			}
		}

	}

	return false;
}

bool down() {
	if(!preferences::use_joystick()) {
		return false;
	}

	foreach(SDL_Joystick* j, joysticks) {
		const int nhats = SDL_JoystickNumHats(j);
		for(int n = 0; n != nhats; ++n) {
			const Uint8 state = SDL_JoystickGetHat(j, n);
			switch(state) {
			case SDL_HAT_DOWN:
			case SDL_HAT_RIGHTDOWN:
			case SDL_HAT_LEFTDOWN:
					return true;
			}
		}

	}

	return false;
}

bool left() {
	if(!preferences::use_joystick()) {
		return false;
	}

	foreach(SDL_Joystick* j, joysticks) {
		const int nhats = SDL_JoystickNumHats(j);
		for(int n = 0; n != nhats; ++n) {
			const Uint8 state = SDL_JoystickGetHat(j, n);
			switch(state) {
			case SDL_HAT_LEFT:
			case SDL_HAT_LEFTDOWN:
			case SDL_HAT_LEFTUP:
					return true;
			}
		}

	}

	return false;
}

bool right() {
	if(!preferences::use_joystick()) {
		return false;
	}

	foreach(SDL_Joystick* j, joysticks) {
		const int nhats = SDL_JoystickNumHats(j);
		for(int n = 0; n != nhats; ++n) {
			const Uint8 state = SDL_JoystickGetHat(j, n);
			switch(state) {
			case SDL_HAT_RIGHT:
			case SDL_HAT_RIGHTDOWN:
			case SDL_HAT_RIGHTUP:
					return true;
			}
		}
	}

	return false;
}

bool button(int n) {
	if(!preferences::use_joystick()) {
		return false;
	}

	foreach(SDL_Joystick* j, joysticks) {
		if(SDL_JoystickGetButton(j, n)) {
			return true;
		}
	}

	return false;
}

int iphone_tilt() {
//#if TARGET_OS_IPHONE
//	return SDL_JoystickGetAxis(joysticks.front(), 1);
//#else
	return 0;
//#endif
}

std::vector<int> get_info() {
	std::vector<int> res;
	res.push_back(joysticks.size());
	foreach(SDL_Joystick* j, joysticks) {
		res.push_back(SDL_JoystickGetAxis(j, 0));
		res.push_back(SDL_JoystickGetAxis(j, 1));
	}
	
	return res;
}

}
