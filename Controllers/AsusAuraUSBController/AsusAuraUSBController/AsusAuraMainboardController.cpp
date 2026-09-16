/*---------------------------------------------------------*\
| AsusAuraMainboardController.cpp                           |
|                                                           |
|   Driver for ASUS Aura mainboard                          |
|                                                           |
|   Martin Hartl (inlart)                       25 Apr 2020 |
|                                                           |
|   This file is part of the OpenRGB project                |
|   SPDX-License-Identifier: GPL-2.0-or-later               |
\*---------------------------------------------------------*/

#include <cstring>
#include <cstdint>
#include <chrono>
#include "AsusAuraMainboardController.h"

#define AURA_MAINBOARD_MODE_ASSERT_WINDOW_MS   30000
#define AURA_MAINBOARD_MODE_ASSERT_INTERVAL_MS 5000

static uint64_t NowMs()
{
    return (uint64_t)(std::chrono::duration_cast<std::chrono::milliseconds>
                     (std::chrono::steady_clock::now().time_since_epoch()).count());
}

AuraMainboardController::AuraMainboardController(hid_device* dev_handle, const char* path, std::string dev_name) : AuraUSBController(dev_handle, path, dev_name), mode(AURA_MODE_DIRECT)
{
    unsigned char num_total_mainboard_leds  = config_table[0x1B];
    unsigned char num_rgb_headers           = config_table[0x1D];
    unsigned char num_addressable_headers   = config_table[0x02];
    unsigned char effect_channel            = 0;

    if(num_total_mainboard_leds < num_rgb_headers)
    {
        num_rgb_headers = 0;
    }

    /*-----------------------------------------------------*\
    | Add mainboard device                                  |
    \*-----------------------------------------------------*/
    if(num_total_mainboard_leds > 0)
    {
        device_info.push_back({effect_channel, 0x04, num_total_mainboard_leds, num_rgb_headers, AuraDeviceType::FIXED});
        effect_channel++;
    }

    /*-----------------------------------------------------*\
    | Add addressable devices                               |
    \*-----------------------------------------------------*/
    for(int i = 0; i < num_addressable_headers; i++)
    {
        device_info.push_back({effect_channel, (unsigned char)i, 0x01, 0, AuraDeviceType::ADDRESSABLE});
        effect_channel++;
    }

    SetGen1();

    mode_registered_ms    = NowMs();
    last_mode_reassert_ms = 0;
}

AuraMainboardController::~AuraMainboardController()
{
}

void AuraMainboardController::SetGen1()
{
    unsigned char usb_buf[65];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up custom command packet                          |
    \*-----------------------------------------------------*/
    usb_buf[0x00] = 0xEC;
    usb_buf[0x01] = 0x52;
    usb_buf[0x02] = 0x53;
    usb_buf[0x03] = 0x00;
    usb_buf[0x04] = 0x01;

    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    hid_write(dev, usb_buf, 65);
}

void AuraMainboardController::SetChannelLEDs(unsigned char channel, RGBColor * colors, unsigned int num_colors)
{
    MaybeReassertMode();

    SendDirect
    (
        device_info[channel].direct_channel,
        num_colors,
        colors
    );

}

void AuraMainboardController::SetMode
    (
    unsigned char   channel,
    unsigned char   mode,
    unsigned char   red,
    unsigned char   grn,
    unsigned char   blu
    )
{
    SetMode(channel, mode, red, grn, blu, false);
}

void AuraMainboardController::SetMode
    (
    unsigned char   channel,
    unsigned char   mode,
    unsigned char   red,
    unsigned char   grn,
    unsigned char   blu,
    bool            shutdown_effect
    )
{
    this->mode = mode;
    RGBColor color = ToRGBColor(red, grn, blu);

    SendEffect(device_info[channel].effect_channel, mode, shutdown_effect);
    last_mode_reassert_ms = NowMs();
    if(mode == AURA_MODE_DIRECT)
    {
        return;
    }

    unsigned char   led_data[60];
    unsigned char   start_led = 0;

    for(std::size_t i = 0; i < channel; ++i)
    {
        start_led += device_info[i].num_leds;
    }

    for(std::size_t led_idx = 0; led_idx < device_info[channel].num_leds; led_idx++)
    {
        led_data[(led_idx * 3) + 0] = RGBGetRValue(color);
        led_data[(led_idx * 3) + 1] = RGBGetGValue(color);
        led_data[(led_idx * 3) + 2] = RGBGetBValue(color);
    }

    SendColor
    (
        channel,
        start_led,
        device_info[channel].num_leds,
        led_data,
        shutdown_effect
    );
}

unsigned short AuraMainboardController::GetMask(int start, int size)
{
    return(((1 << size) - 1) << start);
}

void AuraMainboardController::SendEffect
    (
    unsigned char   channel,
    unsigned char   mode,
    bool            shutdown_effect
    )
{
    unsigned char usb_buf[65];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up message packet                                 |
    \*-----------------------------------------------------*/
    usb_buf[0x00]   = 0xEC;
    usb_buf[0x01]   = AURA_MAINBOARD_CONTROL_MODE_EFFECT;
    usb_buf[0x02]   = channel;
    usb_buf[0x03]   = 0x00;
    usb_buf[0x04]   = shutdown_effect ? 0x01 : 0x00;
    usb_buf[0x05]   = mode;

    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    hid_write(dev, usb_buf, 65);
}

void AuraMainboardController::SendColor
    (
    unsigned char   /*channel*/,
    unsigned char   start_led,
    unsigned char   led_count,
    unsigned char*  led_data,
    bool            shutdown_effect
    )
{
    unsigned short  mask = GetMask(start_led, led_count);
    unsigned char   usb_buf[65];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up message packet                                 |
    \*-----------------------------------------------------*/
    usb_buf[0x00]   = 0xEC;
    usb_buf[0x01]   = AURA_MAINBOARD_CONTROL_MODE_EFFECT_COLOR;
    usb_buf[0x02]   = mask >> 8;
    usb_buf[0x03]   = mask & 0xff;
    usb_buf[0x04]   = shutdown_effect ? 0x01 : 0x00;

    /*-----------------------------------------------------*\
    | Copy in color data bytes                              |
    \*-----------------------------------------------------*/
    memcpy(&usb_buf[0x05 + 3 * start_led], led_data, led_count * 3);

    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    hid_write(dev, usb_buf, 65);
}

void AuraMainboardController::SendCommit()
{
    unsigned char usb_buf[65];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up message packet                                 |
    \*-----------------------------------------------------*/
    usb_buf[0x00]   = 0xEC;
    usb_buf[0x01]   = AURA_MAINBOARD_CONTROL_MODE_COMMIT;
    usb_buf[0x02]   = 0x55;

    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    hid_write(dev, usb_buf, 65);
}

void AuraMainboardController::MaybeReassertMode()
{
    /*-----------------------------------------------------*\
    | Only re-arm Direct mode: it is a no-op, other modes   |
    | could restart a running animation                     |
    \*-----------------------------------------------------*/
    if(mode != AURA_MODE_DIRECT)
    {
        return;
    }

    uint64_t now = NowMs();

    /*------------------------------------------------------*\
    | Only during the bounded window after (re-)registration |
    \*------------------------------------------------------*/
    if((now - mode_registered_ms) > AURA_MAINBOARD_MODE_ASSERT_WINDOW_MS)
    {
        return;
    }

    /*-----------------------------------------------------*\
    | At most once every 5 s                                |
    \*-----------------------------------------------------*/
    if((now - last_mode_reassert_ms) < AURA_MAINBOARD_MODE_ASSERT_INTERVAL_MS)
    {
        return;
    }

    last_mode_reassert_ms = now;

    for(std::size_t device_idx = 0; device_idx < device_info.size(); device_idx++)
    {
        SendEffect(device_info[device_idx].effect_channel, mode, false);
    }
}
