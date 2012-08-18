/* Copyright (C) 2012, dark_skeleton (d-rez) <dark.skeleton@gmail.com> 
   Copyright (C) 2005-2012, Thorvald Natvig <thorvald@natvig.com>

   All rights reserved.
 
   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met: 

   - Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.
   - Redistributions in binary form must reproduce the above copyright notice,
     this list of conditions and the following disclaimer in the documentation
     and/or other materials provided with the distribution.
   - Neither the name of the Mumble Developers nor the names of its
     contributors may be used to endorse or promote products derived from this
     software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
   A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "../mumble_plugin_win32.h"

/*
	Arrays of bytes to find addresses accessed by respective functions so we don't have to blindly search for addresses after every update
	Remember to disable scanning only the writable memory in CE! We're searching for functions here, not values!
	
	Camera and avatar position function:		89 10 8B 57 04 89 50 04 8B 57 08 89 50 08 DF E0 F6 C4 01
		1) Camera position
		2) Camera position (?)
		3) Avatar position
	Camera front vector function:				46 BF 00 89 15 (disassemble, then go up a few times in disassembly)
	Unit front vector function:					89 1E 8B 5F 04 89 5E 04 8B 7F 08 89 7E 08 8B 7D 10 8D 71 28 
		To find the right front vector pointer, you'll need to make a pointer scan for the address that gets updated only when >you< move, using different characters in different locations. It should leave you with only two possible pointers after just 2 scans.
	Location function:							8D 14 DB 8D 34 90 8D 4E 14 (target instruction is the one above the one you find)
	Area function:								A3 58 11 A3 00 E8 B8 02 00 00 BA 37 00 00 00 8B CE

	Valid addresses from build b36001

	Camera position vector address:				0xa30274
	Avatar position vector address:				0xa302a4
	Camera front vector address:				0xbf46b8
	Avatar front vector address:				0xd55610 -> +0x8 --> +0x0 -> +0x1c
	Location:									0xa3fa08
	Area:										0xa31158

	No need to care about top vector since the game doesn't use it anyway
	Context is defined based on location + area combo. This is not enough to be unique in most cases (it doesn't let us distinguish between districts and servers), but it's better than nothing
	
	Location Pointer:
		25 or 26: Explorable Area / Mission
		26 or 27: Town
		0 when logged in / in character select / in loading screen
		1 when not logged in (in login screen)
	
	Area Pointer: 
		This is a 4-byte decimal stating which area we are in. Note however, that some missions have the same area assigned as cities, therefore we need our Location Pointer to distinguish where we are exactly to specify context properly.

*/

static BYTE *camptr = (BYTE *) 0xa30274;
static BYTE *posptr = (BYTE *) 0xa302a4;
static BYTE *camfrontptr = (BYTE *) 0xbf46b8;
static BYTE *frontptr_ = (BYTE *) 0xd55610;
static BYTE *frontptr;

static BYTE *locationptr = (BYTE *) 0xa3fa08;
static BYTE *areaptr = (BYTE *) 0xa31158;

static char prev_location;
static int  prev_areaid;

static bool calcout(float *pos, float *front, float *cam, float *camfront, float *opos, float *ofront, float *ocam, float *ocamfront) {

	// Seems Guild Wars is in... inches, yeah :) ---> same as in GW2, proof here: http://www.guildwars2guru.com/topic/21519-reddit-ama-all-questions-answers (question #31)
	// coordinate Y is swapped with Z
	// Y is negative (looks like somewhere underground is 0.00 and land is for example -120. When we climb up a hill, it decreases (e.g. -130), and when we descent, it goes higher (e.g. -100)

	opos[0] = pos[0] / 39.37f;
	opos[1] = -pos[2] / 39.37f;
	opos[2] = pos[1] / 39.37f;

	ocam[0] = cam[0] / 39.37f;
	ocam[1] = -cam[2] / 39.37f;
	ocam[2] = cam[1] / 39.37f;

	ofront[0] = front[0];
	ofront[1] = -front[2];
	ofront[2] = front[1];

	ocamfront[0] = camfront[0];
	ocamfront[1] = -camfront[2];
	ocamfront[2] = camfront[1];

	return true;
}

static bool refreshPointers(void)
{
	frontptr = NULL;

	frontptr = peekProc<BYTE *>(frontptr_);
	if (!frontptr)
		return false;
	frontptr = peekProc<BYTE *>(frontptr + 0x8);
	if (!frontptr)
		return false;
	frontptr = peekProc<BYTE *>(frontptr);
	if (!frontptr)
		return false;
	frontptr = frontptr + 0x1c;

	return true;
}

static int fetch(float *avatar_pos, float *avatar_front, float *avatar_top, float *camera_pos, float *camera_front, float *camera_top, std::string &context, std::wstring &/*identity*/) {
	for (int i=0; i<3; i++)
		avatar_pos[i] = avatar_front[i] = avatar_top[i] = camera_pos[i] = camera_front[i] = camera_top[i] = 0.0f;

	bool ok, ok_p;
	float cam[3], pos[3], front[3], camfront[3];
	char location;
	int areaid;

	ok_p = refreshPointers(); // yes, we need to do this pretty often since the pointer gets wiped and changed evey time you leave a world instance (that means on loading screens etc)

	ok = peekProc(camptr, cam) &&
		 peekProc(posptr, pos) &&
		 peekProc(camfrontptr, camfront) &&
		 peekProc(frontptr, front) &&
		 peekProc(locationptr, &location, 1) &&
		 peekProc(areaptr, &areaid, 4);

	if (!ok)
		return false; // we can't read some or all of required data, unlink
	
	if (!ok_p) {
		context.clear();
		return true; // don't report positional data but stay linked to avoid unnecessary unlinking on loading screens
	}

	calcout(pos, front, cam, camfront, avatar_pos, avatar_front, camera_pos, camera_front);

	if (areaid != prev_areaid || location != prev_location) {
		context.clear();

		prev_areaid = areaid;
		prev_location = location;

		char buffer[50];
		sprintf_s(buffer, sizeof(buffer), "{\"instance\": \"%d:%d\"}", areaid, static_cast<int>(location));
		context.assign(buffer);
	}
	return true;
}

static int trylock(const std::multimap<std::wstring, unsigned long long int> &pids) {

	if (! initialize(pids, L"Gw.exe"))
		return false;

	float cam[3], pos[3], front[3],camfront[3], top[3], camtop[3];
	std::string context;
	std::wstring identity;

	prev_areaid = 0;
	prev_location = 0;

	if (fetch(pos, front, top, cam, camfront, camtop, context, identity)) {
		prev_areaid = 0;
		prev_location = 0; // we need to do this again since fetch() above overwrites this (which results in empty context until next change)
		return true;
	} else {
		generic_unlock();
		return false;
	}
}

static const std::wstring longdesc() {
	return std::wstring(L"Supports Guild Wars build 36,001 with partial context support.");
}

static std::wstring description(L"Guild Wars b36001");
static std::wstring shortname(L"Guild Wars");

static int trylock1() {
	return trylock(std::multimap<std::wstring, unsigned long long int>());
}

static MumblePlugin gwplug = {
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

static MumblePlugin2 gwplug2 = {
	MUMBLE_PLUGIN_MAGIC_2,
	MUMBLE_PLUGIN_VERSION,
	trylock
};

extern "C" __declspec(dllexport) MumblePlugin *getMumblePlugin() {
	return &gwplug;
}

extern "C" __declspec(dllexport) MumblePlugin2 *getMumblePlugin2() {
	return &gwplug2;
}
