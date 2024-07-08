// Copyright 2024 The Mumble Developers. All rights reserved.
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file at the root of the
// Mumble source tree or at <https://www.mumble.info/LICENSE>.

#include "ProcessWindows.h"
#include "MumblePlugin.h"

#include "mumble_positional_audio_utils.h"

#include <cassert>
#include <cstring>

using GroundedHandle = std::tuple< ProcessWindows, procptr_t >;

static std::unique_ptr< GroundedHandle > handle;

/* the indexes of these float[3] from the game are
 *   0: south low, north high
 *   1: west low, east high
 *   2: altitude, high toward sky, goes up when you jump */
struct GroundedCam {
	/* 940 */ float top[3];
	std::uint8_t _unused1[4];
	/* 950 */ float front[3];
	std::uint8_t _unused2[4 * 40];
	/* 9fc */ float pos[3];
};

static_assert(sizeof(struct GroundedCam) == 200, "GroundedCam struct has unexpected size");

constexpr float unreal_to_mumble_units(float unreal) {
	return unreal / 100.0f;
}

float float3_magnitude(float f[3]) {
	return sqrtf(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
}

bool float3_is_unit(float f[3]) {
	const float err = 0.001f;
	const float mag = float3_magnitude(f);
	return mag > (1.0f - err) && mag < (1.0f + err);
}


mumble_error_t mumble_init(uint32_t) {
	return MUMBLE_STATUS_OK;
}

void mumble_shutdown() {
}

MumbleStringWrapper mumble_getName() {
	static const char name[] = "Grounded";

	MumbleStringWrapper wrapper;
	wrapper.data           = name;
	wrapper.size           = strlen(name);
	wrapper.needsReleasing = false;

	return wrapper;
}

MumbleStringWrapper mumble_getDescription() {
	static const char description[] = "Positional audio support for Grounded. Steam release version 1.4.3.4578.";

	MumbleStringWrapper wrapper;
	wrapper.data           = description;
	wrapper.size           = strlen(description);
	wrapper.needsReleasing = false;

	return wrapper;
}

MumbleStringWrapper mumble_getAuthor() {
	static const char author[] = "MumbleDevelopers";

	MumbleStringWrapper wrapper;
	wrapper.data           = author;
	wrapper.size           = strlen(author);
	wrapper.needsReleasing = false;

	return wrapper;
}

mumble_version_t mumble_getAPIVersion() {
	return MUMBLE_PLUGIN_API_VERSION;
}

void mumble_registerAPIFunctions(void *) {
}

void mumble_releaseResource(const void *) {
}

mumble_version_t mumble_getVersion() {
	return { 1, 0, 0 };
}

uint32_t mumble_getFeatures() {
	return MUMBLE_FEATURE_POSITIONAL;
}

uint8_t mumble_initPositionalData(const char *const *programNames, const uint64_t *programPIDs, size_t programCount) {
	const std::string exename = "Maine-Win64-Shipping.exe";

	for (size_t i = 0; i < programCount; ++i) {
		if (programNames[i] != exename) {
			continue;
		}

		ProcessWindows proc(programPIDs[i], programNames[i]);

		if (!proc.isOk()) {
			continue;
		}

		const Modules &modules = proc.modules();
		const auto iter        = modules.find(exename);

		if (iter == modules.cend()) {
			continue;
		}

		/* Look up camera positional data through pointer chain */

		procptr_t p = iter->second.baseAddress();

		if (!(p = proc.peekPtr(p + 0x0612cb38))) {
			continue;
		}

		if (!(p = proc.peekPtr(p + 0x0))) {
			continue;
		}

		if (!(p = proc.peekPtr(p + 0x8))) {
			continue;
		}

		p += 0x8c0;

		GroundedCam _cam;

		if (!(proc.peek(p, _cam))) {
			continue;
		}

		handle = std::make_unique< GroundedHandle >(std::move(proc), p);

		return MUMBLE_PDEC_OK;
	}

	return MUMBLE_PDEC_ERROR_TEMP;
}

void mumble_shutdownPositionalData() {
	handle.reset();
}

bool mumble_fetchPositionalData(float *avatarPos, float *avatarDir, float *avatarAxis, float *cameraPos,
								float *cameraDir, float *cameraAxis, const char **contextPtr,
								const char **identityPtr) {
	*contextPtr  = "";
	*identityPtr = "";

	ProcessWindows &proc = std::get< 0 >(*handle);
	procptr_t camAddr    = std::get< 1 >(*handle);

	GroundedCam cam;

	if (!proc.peek< GroundedCam >(camAddr, cam)) {
		std::fill_n(avatarPos, 3, 0.f);
		std::fill_n(avatarDir, 3, 0.f);
		std::fill_n(avatarAxis, 3, 0.f);

		std::fill_n(cameraPos, 3, 0.f);
		std::fill_n(cameraDir, 3, 0.f);
		std::fill_n(cameraAxis, 3, 0.f);

		return false;
	}

	/* We expect top and front to be unit vectors in the game. */
	assert(float3_is_unit(cam.top));
	assert(float3_is_unit(cam.front));

	avatarAxis[0] = cameraAxis[0] = -cam.top[0];
	avatarAxis[1] = cameraAxis[1] = cam.top[2];
	avatarAxis[2] = cameraAxis[2] = -cam.top[1];

	avatarDir[0] = cameraDir[0] = -cam.front[0];
	avatarDir[1] = cameraDir[1] = cam.front[2];
	avatarDir[2] = cameraDir[2] = -cam.front[1];

	avatarPos[0] = cameraPos[0] = unreal_to_mumble_units(cam.pos[0]);
	avatarPos[1] = cameraPos[1] = unreal_to_mumble_units(cam.pos[2]);
	avatarPos[2] = cameraPos[2] = unreal_to_mumble_units(cam.pos[1]);

	return true;
}
