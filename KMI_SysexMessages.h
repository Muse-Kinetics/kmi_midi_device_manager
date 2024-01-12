// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef KMI_SYSEXMESSAGES_H
#define KMI_SYSEXMESSAGES_H

#include "midi.h"
#include "KMI_DevData.h"


// ************************************************
// SysEx Messages


// Universal Device ID and Firmware Request
extern unsigned char _sx_id_req_standard[6];

// Universal Device ID and Firmware Reply (kmi is usually "sysex channel" 0)
extern unsigned char _sx_id_reply_standard[5];

extern unsigned char _sx_ack_loop_test[6];

// SoftStep Firmware Request
extern unsigned char _fw_req_softstep[67];

//// SoftStep alternate firmware version request string
//extern unsigned char _fw_req_softstep[] =
//{
//    0xF0, 0x00, 0x1B, 0x48, 0x7A, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x01, 0x00, 0x09, 0x00, 0x0B, 0x2B, 0x3A, 0x00, 0x10, 0x08, 0x01, 0x01, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x7B, 0x69, 0x00, 0x00, 0x00, 0x00, 0x02, 0xF7
//};

// SoftStep Firmware Reply
extern unsigned char _fw_reply_softstep[4];

// 12Step Firmware Request
extern unsigned char _fw_req_12step[67];

// 12Step Firmware Reply
extern unsigned char _fw_reply_12step[5];

// ***** QuNeo Specific commands *************************
// QuNeo swap LEDs
extern unsigned char _quneo_swap_leds[17];

// QuNeo query board ID
extern unsigned char _quneo_board_id_query[6];

// EB TODO - what are these for?
extern unsigned char _quneo_toggleProgramChangeInSysExData[17];

// EB TODO - what are these for?
extern unsigned char _quneo_toggleProgramChangeOutSysExData[17];


// *********************************************************
// Enter bootloader commands - TODO see if this can be done programatically or in an array

// 12 Step
extern unsigned char _bl_12step[21];

// SoftStep
extern unsigned char _bl_softstep[21];

// QuNexus
extern unsigned char _bl_qunexus[17];

// QuNeo
extern unsigned char _bl_quneo[18];


// *********************************************************
#endif
