// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef MIDI_H
#define MIDI_H

/* KMI midi.h

  Ported from KMI firmware, this header contains #defines for common MIDI messages

  This file should only contain #defines, and can be #included anywhere it is needed

*/

enum MIDI_CHANNELS
{
    MIDI_CH_1,		// 0
    MIDI_CH_2,
    MIDI_CH_3,
    MIDI_CH_4,
    MIDI_CH_5,
    MIDI_CH_6,
    MIDI_CH_7,
    MIDI_CH_8,
    MIDI_CH_9,
    MIDI_CH_10,
    MIDI_CH_11,
    MIDI_CH_12,
    MIDI_CH_13,
    MIDI_CH_14,
    MIDI_CH_15,
    MIDI_CH_16,
    NUM_MIDI_CHANNELS // 16
};

#define	MIDI_NOTE_OFF			0x80
#define	MIDI_NOTE_ON			0x90
#define	MIDI_NOTE_AFTERTOUCH	0xA0
#define	MIDI_CONTROL_CHANGE		0xB0
#define	MIDI_PROG_CHANGE		0xC0
#define	MIDI_CHANNEL_PRESSURE	0xD0
#define	MIDI_PITCH_BEND			0xE0
#define	MIDI_SX_START			0xF0
#define	MIDI_MTC				0xF1
#define	MIDI_SONG_POSITION		0xF2
#define	MIDI_SONG_SELECT		0xF3
#define	MIDI_TUNE_REQUEST		0xF6
#define MIDI_BANK_CHANGE_LSB	0x20
#define MIDI_BANK_CHANGE_MSB	0x00

#define	MIDI_SX_STOP			0xF7

#define MIDI_RT_CLOCK			0xF8
#define	MIDI_RT_UNDEF1			0xF9
#define MIDI_RT_START			0xFA
#define	MIDI_RT_CONTINUE		0xFB
#define	MIDI_RT_STOP			0xFC
#define	MIDI_RT_UNDEF2			0xFD
#define	MIDI_RT_ACTIVE_SENSE	0xFE
#define	MIDI_RT_RESET			0xFF

#define kmi_id_1 				0x00
#define kmi_id_2 				0x01
#define kmi_id_3				0x5F

// might not have been Chuck but this is our mysterious fourth sysex manufacturer ID number
#define chuck_magic_number		0x7A

#define kmi_family_lsb			0x00
#define kmi_family_msb			0x00

#define SX_UNIVERSAL			0x7E
// the sysex address/id/channel of this device
#define SX_ADDRESS				0x00
// for ID requests where we ignore the address byte
#define SX_ADD_IGNORE			0x7F
#define SX_ADD_ZERO             0x00
#define SX_UNIV_ACK             0x7F

// info request (device id)
#define SX_UNV_INFO				0x06
// device id request
#define SX_UNV_DEVID_REQ		0x01
// device id reply
#define SX_UNV_DEVID_REPLY		0x02

// 12 bytes for device id reply
#define SX_DEVID_SIZE			12

#define	MAX_DEST	1

#define	NOTE_VELOCITY_DEFAULT	0x40

// Reserved CCs
#define RES_CC_ALL_SOUNDS_OFF	0x78
#define RES_CC_RESET_ALL_CC		0x79
#define RES_CC_LOCAL_CONTROL	0x7A
#define RES_CC_ALL_NOTES_OFF	0x7B


// MIDI DATA/RPN/NRPN CCs
#define MIDI_CC_DATA_MSB 		6
#define MIDI_CC_DATA_LSB 		38
#define MIDI_CC_DATA_INC 		96
#define MIDI_CC_DATA_DEC 		97

#define MIDI_CC_NRPN_LSB		98
#define MIDI_CC_NRPN_MSB		99
#define MIDI_CC_RPN_LSB			100
#define MIDI_CC_RPN_MSB			101
#define RPN_NULL                127

#define RPN_PB_RANGE            0
#define RPN_TUNE_FINE           1
#define RPN_TUNE_COARSE         2
#define RPN_TUNE_PROG           3
#define RPN_TUNE_BANK           4

#define RPN_MCM                 6

// MPE
#define MPE_MGR_CH_Z1			0
#define MPE_MGR_CH_Z2			15

#endif // MIDI_H
