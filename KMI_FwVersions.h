// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef KMI_FWVERSIONS_H
#define KMI_FWVERSIONS_H

/* KMI Firmware Versions Header

  Stores the latest KMI firmware versions in uchar arrays

  This file should only be included at the top of your Qt project's mainwindow.cpp

*/

#include "midi.h"

// ************************************************
// Firmware Versions

unsigned char _bl_ver_qunexus[] =
{
    1, 0, 2
};
unsigned char _fw_ver_qunexus[] =
{
    2, 2, 0
};

unsigned char _fw_ver_kboard[] =
{
    1, 0, 1
};

unsigned char _fw_ver_boppad[] =
{
    3, 0, 0
};

// the QuNeo boot version is two bytes, ie 1.3, transmitted in LSB MSB order
unsigned char _bl_ver_quneo[] =
{
    1, 3
};
// the QuNeo firmware version is two bytes, transmitted in LSB MSB order
// However the MSB breaks out into 1.x. So 0x17 0x12 = 1.2.30
unsigned char _fw_ver_quneo[] =
{
    1, 2, 31
};

unsigned char _fw_ver_12step[] =
{
    1, 0, 9
};

unsigned char _fw_ver_softstep[] =
{
    2, 0, 6
};

unsigned char _bl_ver_softstep[] =
{
    1, 0, 0
};

// malletStation EM Pro (controller). The ID reply carries only major.minor.patch;
// the 4th "dev" byte (internal 0.1.6.4) is not transmitted, so this is 0.1.6.
unsigned char _fw_ver_empro[] =
{
    0, 1, 6
};

// SoundStation. Internal version 0.1.0.1; ID reply carries 0.1.0. (Version still in flux.)
unsigned char _fw_ver_soundstation[] =
{
    0, 1, 0
};

#endif // KMI_FWVERSIONS_H
