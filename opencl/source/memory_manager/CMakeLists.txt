#
# Copyright (C) 2018-2021 Intel Corporation
#
# SPDX-License-Identifier: MIT
#

set(RUNTIME_SRCS_MEMORY_MANAGER
    ${CMAKE_CURRENT_SOURCE_DIR}/CMakeLists.txt
    ${CMAKE_CURRENT_SOURCE_DIR}${BRANCH_DIR_SUFFIX}/compression_selector_ocl.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/cpu_page_fault_manager_memory_sync.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/mem_obj_surface.h
    ${CMAKE_CURRENT_SOURCE_DIR}/resource_surface.h
)

target_sources(${NEO_STATIC_LIB_NAME} PRIVATE ${RUNTIME_SRCS_MEMORY_MANAGER})
set_property(GLOBAL PROPERTY RUNTIME_SRCS_MEMORY_MANAGER ${RUNTIME_SRCS_MEMORY_MANAGER})
add_subdirectories()
