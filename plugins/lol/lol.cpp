/* Copyright (C) 2012, dark_skeleton (d-rez) <dark.skeleton@gmail.com> 

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

static BYTE *posptr;
static BYTE *camptr;
static BYTE *camfrontptr;
static BYTE *camtopptr;
static BYTE *afrontptr;
static BYTE *atopptr;

static BYTE *hostipptr;
static BYTE *hostportptr;
static BYTE *summonerptr;

static BYTE *gameptr;

static char prev_hostip[16]; // These should never change while the game is running, but just in case...
static int prev_hostport;
static char prev_summoner[17];

static bool calcout(float *pos, float *cam, float *opos, float *ocam) {
	// Seems League of Legends is in centimeters? ;o Well it's not inches for sure :)
	for (int i = 0; i < 3; i++) {
		opos[i] = pos[i] / 100.00f;
		ocam[i] = cam[i] / 100.00f;
	}

	return true;
}

static bool refreshPointers(void) {
	/* Arrays of bytes to find addresses accessed by respective functions so we don't have to blindly search for addresses after every update
	Remember to disable scanning writable memory only in CE! We're searching for functions here, not values!
	Current addresses as of version 1.0.0.142

	Camera position vector address: F3 0F 11 03 F3 0F 10 44 24 14 D9 5C 24 28			:00B4B858
	Camera front vector address: campos+0x14 (offset, not pointer!)
	Camera top vector address: campos+0x20 (offset, not pointer!)

	D9 5F 40 D9 46 04 D9 5F 44 D9 46 08 D9 5F 48 59 C3 CC (non-static! NEEDS POINTER)	:00DFA4E8 
	Avatar front vector address: 		+0x2ab4
	Avatar top vector address: 		+0x2ac0

	D9 9E E8 01 00 00 D9 40 70 D9 9E EC 01 00 00 D9 40 74 D9 9E F0 01 00 00				:02F2DE68 
	Avatar position vector address:		+0x1e8

	IP: Look for a non-unicode string that will contain server's IP. 28 bytes further from IP, there should be server's port
																						:0AF395B8 
	PORT:					+0x1C (offset, not pointer!)
	IDENTITY: Just look for your nickname saved in non-unicode that is static. Length is  16
																						:0AF3957C 
	*/

	posptr = camptr = camfrontptr = camtopptr = afrontptr = atopptr = NULL;
	
	// Camera position
	camptr = (BYTE *)0xB4B858;

	// Camera front
	camfrontptr = camptr + 0x14;

	// Camera top
	camtopptr = camptr + 0x20;

	// Avatar front vector pointer
	BYTE *tmpptr = NULL;
	gameptr = (BYTE *)0xDFA4E8;
	tmpptr = peekProc<BYTE *>(gameptr);	

	if (!tmpptr)
		return false;				// Something went wrong, unlink

	afrontptr = tmpptr + 0x2ab4;
	atopptr = tmpptr + 0x2ac0;
	
	// Avatar position vector
	tmpptr = peekProc<BYTE *>((BYTE *)0x2F2DE68);
	if (!tmpptr)
		return false;				// Something went wrong, unlink

	posptr = tmpptr + 0x1e8;

	// Host IP:PORT. It is kept in 3 places in memory, but 1 of them looks the coolest, so let's use it, ha!
	// IP is kept as text @ hostipptr
	// PORT is kept as a 4-byte decimal value @ hostportptr

	hostipptr = (BYTE *)0xAF395B8;
	hostportptr = hostipptr + 0x1C;

	summonerptr = (BYTE *)0xAF3957C;

	return true;
}

static int fetch(float *avatar_pos, float *avatar_front, float *avatar_top, float *camera_pos, float *camera_front, float *camera_top, std::string &context, std::wstring &identity) {
	for (int i = 0; i < 3; i++)
		avatar_pos[i] = avatar_front[i] = avatar_top[i] = camera_pos[i] = camera_front[i] = camera_top[i] = 0.0f;

	float ipos[3], cam[3];
	int hostport;
	char hostip[16];
	char summoner[17];
	bool ok;

	// Player not in game (or something broke), unlink
	if (!peekProc<BYTE *>(gameptr))
		return false;

	ok = peekProc(camfrontptr, camera_front, 12) &&
		 peekProc(camtopptr, camera_top, 12) &&
		 peekProc(camptr, cam, 12) &&
		 peekProc(posptr, ipos, 12) &&
		 peekProc(afrontptr, avatar_front, 12) &&
		 peekProc(atopptr, avatar_top, 12) &&
		 peekProc(hostipptr, hostip) &&
		 peekProc(hostportptr, &hostport, 4) &&
		 peekProc(summonerptr, summoner);

	if (ok) {
		int res = calcout(ipos, cam, avatar_pos, camera_pos);
		if (res) {
			if (strcmp(hostip, prev_hostip) != 0 || hostport != prev_hostport) {
				context.clear();
				memcpy(prev_hostip, hostip, 16);
				prev_hostport = hostport;

				if (strcmp(hostip, "") != 0) {
					char buffer[50];
					sprintf_s(buffer, 50, "{\"ipport\": \"%s:%d\"}", hostip, hostport);
					context.assign(buffer);
				}
			}
			if (strcmp(summoner, prev_summoner) != 0) {
				identity.clear();
				memcpy(prev_summoner,summoner,17);
				
				if (strcmp(summoner, "") != 0) {
					wchar_t tmp[sizeof(summoner)];
					mbstowcs_s(NULL,tmp,summoner,sizeof(summoner));
					wchar_t buffer[50];
					swprintf_s(buffer, 50, L"{\"summoner\": \"%s\"}", tmp);
					identity.assign(buffer);
				}
			}
		}
		return res;
	}

	return false;
}

static int trylock(const std::multimap<std::wstring, unsigned long long int> &pids) {
	if (! initialize(pids, L"League of Legends.exe"))
		return false;

	float pos[3], opos[3];
	float cam[3], ocam[3];

	// unlink plugin if this fails
	if (!refreshPointers()) {
		generic_unlock();
		return false;
	}

	if (calcout(pos,cam,opos,ocam)) { // make sure values are OK
		*prev_hostip = '\0';
		prev_hostport = 0;
		*prev_summoner = '\0';
		return true;
	}

	generic_unlock();
	return false;
}

static const std::wstring longdesc() {
	return std::wstring(L"Supports League of Legends v1.0.0.142 with context and identity support.");
}

static std::wstring description(L"League of Legends (v1.0.0.142)");
static std::wstring shortname(L"League of Legends");

static int trylock1() {
	return trylock(std::multimap<std::wstring, unsigned long long int>());
}

static MumblePlugin lolplug = {
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

static MumblePlugin2 lolplug2 = {
	MUMBLE_PLUGIN_MAGIC_2,
	MUMBLE_PLUGIN_VERSION,
	trylock
};

extern "C" __declspec(dllexport) MumblePlugin *getMumblePlugin() {
	return &lolplug;
}

extern "C" __declspec(dllexport) MumblePlugin2 *getMumblePlugin2() {
	return &lolplug2;
}
