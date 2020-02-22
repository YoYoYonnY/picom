// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2018 Yuxuan Shui <yshuiv7@gmail.com>

#pragma once

/// Some common types

#include <stdint.h>

typedef struct buf {
	void *data;
	size_t size;
} buf_t;
#define BUF_NULL ((buf_t){ NULL, 0 })

/// Enumeration type to represent switches.
typedef enum {
	OFF = 0,        // false
	ON,             // true
	UNSET
} switch_t;

enum blur_method {
	BLUR_METHOD_NONE = 0,
	BLUR_METHOD_KERNEL,
	BLUR_METHOD_BOX,
	BLUR_METHOD_GAUSSIAN,
	BLUR_METHOD_INVALID,
};

/// A structure representing margins around a rectangle.
typedef struct {
	int top;
	int left;
	int bottom;
	int right;
} margin_t;

struct color {
	double red, green, blue, alpha;
};

typedef uint32_t opacity_t;

#define MARGIN_INIT                                                                      \
	{ 0, 0, 0, 0 }
