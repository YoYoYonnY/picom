#pragma once

#include <stdbool.h>
#include "common.h"
#include "../common.h"

bool glx_blur_dst(session_t *ps, int dx, int dy, int width, int height, float z,
				  GLfloat factor_center, const region_t *reg_tgt, glx_blur_cache_t *pbc);
