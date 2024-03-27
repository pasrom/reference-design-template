# Copyright (c) 2022-2023 Golioth, Inc.
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(rd_template)

if(CONFIG_SPS30)
    zephyr_include_directories(${WEST_TOPDIR}/app/external/sensirion/embedded-common/common)
    zephyr_include_directories(${WEST_TOPDIR}/app/external/sensirion/embedded-common/i2c)
    zephyr_include_directories(${WEST_TOPDIR}/app/external/sensirion/embedded-sps/sps-common)
    zephyr_include_directories(${WEST_TOPDIR}/app/external/sensirion/embedded-sps/sps30-i2c)
endif()

target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/app_rpc.c)
target_sources(app PRIVATE src/app_settings.c)
target_sources(app PRIVATE src/app_state.c)
target_sources(app PRIVATE src/app_sensors.c)
if(CONFIG_SPS30)
    target_sources(app PRIVATE src/sensors.c)
    target_sources(app PRIVATE ${WEST_TOPDIR}/app/external/sensirion/embedded-common/common/sensirion_common.c)
    target_sources(app PRIVATE ${WEST_TOPDIR}/app/external/sensirion/embedded-common/i2c/sensirion_i2c_hal.c)
    target_sources(app PRIVATE ${WEST_TOPDIR}/app/external/sensirion/embedded-common/i2c/sensirion_i2c.c)
    target_sources(app PRIVATE ${WEST_TOPDIR}/app/external/sensirion/embedded-sps/sps30-i2c/sps30.c)
endif()
add_subdirectory_ifdef(CONFIG_ALUDEL_BATTERY_MONITOR src/battery_monitor)
