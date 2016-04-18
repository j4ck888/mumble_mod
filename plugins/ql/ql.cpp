// Copyright 2016 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "../mumble_plugin_win32.h"  

static int fetch(float *avatar_pos, float *avatar_front, float *avatar_top, float *camera_pos, float *camera_front, float *camera_top, std::string &context, std::wstring &identity) {
	for (int i=0;i<3;i++) {
		avatar_pos[i] = avatar_front[i] = avatar_top[i] = camera_pos[i] = camera_front[i] = camera_top[i] = 0.0f;
	}

	char state, host[22];
	BYTE team;
	bool ok;

	// Create containers to stuff our raw data into, so we can convert it to Mumble's coordinate system
	float pos_corrector[3];
	float viewHor, viewVer;

	// Peekproc and assign game addresses to our containers, so we can retrieve positional data
	ok = peekProc((BYTE *) pModule + 0x0188248, &state, 1) && // Magical state value: 1 when in-game and 0 when not
		peekProc((BYTE *) pModule + 0x10728C4, &pos_corrector, 12) &&
		peekProc((BYTE *) pModule + 0x1072954, &viewHor, 4) &&
		peekProc((BYTE *) pModule + 0x1072950, &viewVer, 4) &&
		peekProc((BYTE *) pModule + 0x0E4A638, host) && // Server value: "IP:Port" when in a remote server, "localhost" when on a local server.
		peekProc((BYTE *) pModule + 0x106CE6C, team); // Team value: 0 when in a FFA game (no team); 1 when in Red team; 2 when in Blue team; 3 when in Spectators.
	
	if (! ok) {
		return false;
	}

	if (state == 0) { // If not in-game
		context.clear(); // Clear context
		identity.clear(); // Clear identity
		
		return true; // This results in all vectors beeing zero which tells Mumble to ignore them.
	}

	if (state == 1 && team == 3) { // If in-game as spectator
		/*
		Set to 0 avatar and camera values.
		*/
		for (int i=0;i<3;i++) {
			camera_pos[i] =  camera_front[i] = camera_top[i] = avatar_pos[i] = avatar_front[i] = avatar_top[i] = 0.0f;
		}
		/*
		Set team to SPEC.
		*/
		std::wostringstream oidentity;
		oidentity << "{\"team\": \"SPEC\"}";
		identity = oidentity.str();
		
		return true; // This results in all vectors beeing zero which tells Mumble to ignore them.
	}

	host[sizeof(host) - 1] = '\0';
	std::string Server(host);
	// This string can be either "xxx.xxx.xxx.xxx:yyyyy" (or shorter), "localhost" or "" (empty) when loading. Hence 22 size for char.
	if (!Server.empty()) {
		if (Server.find("localhost") == std::string::npos) {
			std::ostringstream newcontext;
			newcontext << "{\"ipport\": \"" << Server << "\"}";
			context = newcontext.str();
		}

		std::wostringstream oidentity;
		if (team == 0)
			oidentity << "{\"team\": \"FFA\"}";
		else if (team == 1)
			oidentity << "{\"team\": \"RED\"}";
		else if (team == 2)
			oidentity << "{\"team\": \"BLUE\"}";
		else if (team == 3)
			oidentity << "{\"team\": \"SPEC\"}";
		identity = oidentity.str();
	}
	
	/*
	Game | Mumble
	X    | Y
	Y    | Z
	Z    | X
	*/
	avatar_pos[0] = pos_corrector[1];
	avatar_pos[1] = pos_corrector[2];
	avatar_pos[2] = pos_corrector[0];
	
	// Scale to meters
	for (int i=0;i<3;i++)
		avatar_pos[i]/=70.0f;
	
	viewVer *= static_cast<float>(M_PI / 180.0f);
	viewHor *= static_cast<float>(M_PI / 180.0f);

	avatar_front[0] = -sin(viewHor) * cos(viewVer);
	avatar_front[1] = -sin(viewVer);
	avatar_front[2] = cos(viewHor) * cos(viewVer);

	avatar_top[0] = -sin(viewHor) * cos(viewVer);
	avatar_top[1] = -sin(viewVer);
	avatar_top[2] = cos(viewHor) * cos(viewVer);
	
	for (int i=0;i<3;i++) {
		camera_pos[i] = avatar_pos[i];
		camera_front[i] = avatar_front[i];
		camera_top[i] = avatar_top[i];
	}

	return true;
}

static int trylock(const std::multimap<std::wstring, unsigned long long int> &pids) {

	if (! initialize(pids, L"quakelive_steam.exe")) { // Link the game executable
		return false;
	}

	// Check if we can get meaningful data from it
	float apos[3], afront[3], atop[3], cpos[3], cfront[3], ctop[3];
	std::wstring sidentity;
	std::string scontext;

	if (fetch(apos, afront, atop, cpos, cfront, ctop, scontext, sidentity)) {
		return true;
	} else {
		generic_unlock();
		return false;
	}
}

static const std::wstring longdesc() {
	return std::wstring(L"Supports Quake Live version 1067 with context and identity support."); // Plugin long description
}

static std::wstring description(L"Quake Live (v1067)"); // Plugin short description
static std::wstring shortname(L"Quake Live"); // Plugin short name

static int trylock1() {
	return trylock(std::multimap<std::wstring, unsigned long long int>());
}

static MumblePlugin qlplug = {
	MUMBLE_PLUGIN_MAGIC,
	description,
	shortname,
	NULL,
	NULL,
	trylock1,
	generic_unlock,
	longdesc,
	fetch
};

static MumblePlugin2 qlplug2 = {
	MUMBLE_PLUGIN_MAGIC_2,
	MUMBLE_PLUGIN_VERSION,
	trylock
};

extern "C" __declspec(dllexport) MumblePlugin *getMumblePlugin() {
	return &qlplug;
}

extern "C" __declspec(dllexport) MumblePlugin2 *getMumblePlugin2() {
	return &qlplug2;
}
