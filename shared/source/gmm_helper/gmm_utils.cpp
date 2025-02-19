/*
 * Copyright (C) 2020-2021 Intel Corporation
 *
 * SPDX-License-Identifier: MIT
 *
 */

#
#include "shared/source/gmm_helper/gmm.h"
#include "shared/source/helpers/surface_format_info.h"

using namespace NEO;

void Gmm::applyAppResource(StorageInfo &storageInfo) {}
void Gmm::applyMemoryFlags(bool systemMemoryPool, StorageInfo &storageInfo) { this->useSystemMemoryPool = systemMemoryPool; }
