// SPDX-License-Identifier: MPL-2.0
// Copyright (c) Yuxuan Shui <yshuiv7@gmail.com>

#pragma once

#include <stdio.h>
#include <xcb/xcb.h>

struct session;
struct backend_base;

// A list of known driver quirks:
// *  NVIDIA driver doesn't like seeing the same pixmap under different
//    ids, so avoid naming the pixmap again when it didn't actually change.

#define DEFINE_DRIVERS(DEFINE_DRIVER) \
	DEFINE_DRIVER(DRIVER_AMDGPU,      0x01, "AMDGPU") /* AMDGPU for DDX, radeonsi for OpenGL */ \
	DEFINE_DRIVER(DRIVER_RADEON,      0x02, "Radeon") /* ATI for DDX, mesa r600 for OpenGL */ \
	DEFINE_DRIVER(DRIVER_FGLRX,       0x04, "fglrx") \
	DEFINE_DRIVER(DRIVER_NVIDIA,      0x08, "NVIDIA") \
	DEFINE_DRIVER(DRIVER_NOUVEAU,     0x10, "nouveau") \
	DEFINE_DRIVER(DRIVER_INTEL,       0x20, "Intel") \
	DEFINE_DRIVER(DRIVER_MODESETTING, 0x40, "modesetting")

/// A list of possible drivers.
/// The driver situation is a bit complicated. There are two drivers we care about: the
/// DDX, and the OpenGL driver. They are usually paired, but not always, since there is
/// also the generic modesetting driver.
/// This enum represents _both_ drivers.
enum driver {
#define DEFINE_DRIVER(enumname, mask, name) enumname = mask,
	DEFINE_DRIVERS(DEFINE_DRIVER)
#undef DEFINE_DRIVER
};

/// Return a list of all drivers currently in use by the X server.
/// Note, this is a best-effort test, so no guarantee all drivers will be detected.
enum driver detect_driver(xcb_connection_t *, struct backend_base *, xcb_window_t);

// Print driver names to stdout, for diagnostics
static inline void print_drivers(enum driver drivers) {
	const char *prefix = "";
#define DEFINE_DRIVER(enumname, mask, name) if (drivers & mask) { printf("%s%s", prefix, name); prefix = ", "; }
	DEFINE_DRIVERS(DEFINE_DRIVER)
#undef DEFINE_DRIVER
	printf("\n");
}
