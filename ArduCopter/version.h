/////////////////////////////////////////////////////////////////
//Modified by Leading Edge Aerial Technologies, LLC. (Feb 2021)//
/////////////////////////////////////////////////////////////////

#pragma once

#ifndef FORCE_VERSION_H_INCLUDE
#error version.h should never be included directly. You probably want to include AP_Common/AP_FWVersion.h
#endif

#include "ap_version.h"

#define THISFIRMWARE "PrecisionVision V1.0.0"

// the following line is parsed by the autotest scripts
#define FIRMWARE_VERSION 4,0,4,FIRMWARE_VERSION_TYPE_RC

#define FW_MAJOR 1
#define FW_MINOR 0
#define FW_PATCH 0
#define FW_TYPE FIRMWARE_VERSION_TYPE_RC
