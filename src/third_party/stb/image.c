// ps5-homebrew-ui - PNG and JPEG decoding for the artwork worker.
// Copyright (C) 2026 BlackBearReloaded
// SPDX-License-Identifier: GPL-3.0-or-later

#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_STDIO
#define STBI_NO_SIMD
#define STBI_ONLY_JPEG
#define STBI_ONLY_PNG
#define STBI_MAX_DIMENSIONS 2048
#include "stb_image.h"
