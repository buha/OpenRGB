/*---------------------------------------------------------*\
| AsusAuraMainboardController.h                             |
|                                                           |
|   Driver for ASUS Aura mainboard                          |
|                                                           |
|   Martin Hartl (inlart)                       25 Apr 2020 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#pragma once

#include <string>
#include <hidapi.h>
#include "RGBController.h"
#include "AsusAuraUSBController.h"

enum
{
    AURA_MAINBOARD_CONTROL_MODE_EFFECT            = 0x35,     /* Effect control mode                  */
    AURA_MAINBOARD_CONTROL_MODE_EFFECT_COLOR      = 0x36,     /* Effect color control mode            */
    AURA_MAINBOARD_CONTROL_MODE_COMMIT            = 0x3F,     /* Commit mode                          */
};

class AuraMainboardController : public AuraUSBController
{
public:
    AuraMainboardController(hid_device* dev_handle, const char* path, std::string dev_name);
    ~AuraMainboardController();

    void SetChannelLEDs
        (
        unsigned char   channel,
        RGBColor *      colors,
        unsigned int    num_colors
        );

    void SetMode
        (
        unsigned char   channel,
        unsigned char   mode,
        unsigned char   red,
        unsigned char   grn,
        unsigned char   blu
        );

    void SetMode
        (
        unsigned char   channel,
        unsigned char   mode,
        unsigned char   red,
        unsigned char   grn,
        unsigned char   blu,
        bool            shutdown_effect
        );

    void SendCommit();

private:
    unsigned int mode;

    /*-----------------------------------------------------*\
    | The device may silently drop the first effect-mode   |
    | write right after USB (re-)enumeration, leaving the  |
    | driver in "Direct" while the hardware plays its      |
    | built-in effect. Re-assert Direct mode at a low rate |
    | for a bounded window after registration so the       |
    | device converges on the driver's state.              |
    \*-----------------------------------------------------*/
    void     MaybeReassertMode();

    uint64_t mode_registered_ms;
    uint64_t last_mode_reassert_ms;

    unsigned short GetMask(int start, int size);

    void SendEffect
        (
        unsigned char   channel,
        unsigned char   mode,
        bool            shutdown_effect
        );

    void SendColor
        (
        unsigned char   channel,
        unsigned char   start_led,
        unsigned char   led_count,
        unsigned char*  led_data,
        bool            shutdown_effect
        );

    void SetGen1();
};
