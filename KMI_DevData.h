// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef KMI_DEV_DATA_H
#define KMI_DEV_DATA_H

/* KMI Device Data Header

  Contains KMI device information, such as SysEx/USB PIDs, portnames, etc

  This file should only contain #defines, and can be #included at the top of KMI_dm.cpp

*/

#include "qglobal.h" // needed for OS defines
#include "midi.h"

// USB and SysEx Product IDs

#define PID_AUX             0
#define PID_STRINGPORT      1
#define PID_SOFTSTEP1       10      // two ports that get translated (SSCOM)
#define PID_SOFTSTEP2       11      // two ports that get translated (SSCOM)
#define PID_SOFTSTEP_USB    12      // SoftStep post SSCOM, uses 11 as MIDI PID, hard coded portnames, uses MIDI PID 11
#define PID_SOFTSTEP3       13      // hard coded portnames
#define PID_SOFTSTEP_BL     14      // test this
#define PID_12STEP1         20      // legacy ports that get translated
#define PID_12STEP_BL       21      // hard coded portnames
#define PID_12STEP2         22      // hard coded portnames
#define PID_QUNEXUS         25      // legacy ports that get translated
#define PID_KBOARD          26      // single port
#define PID_APPL_CBL        27
#define PID_QUNEO           30      // single port
#define PID_ROGUE           31      // single port
#define PID_KMIX            35      // hard coded port names
#define PID_KMIX_CTL        36
#define PID_KBP4            37      // hard coded port names
#define PID_KBP4_BL         38
#define PID_EM1             39      // test but likely hard coded
#define PID_EM1_BL          40

#define PID_BOPPAD          117
#define PID_BOPPAD_BL       118 // this may be USB only, confirm if sysex changes

// **********************************************************************
// define all of the USB OS specific midi port names used in KMI products
// **********************************************************************

// QUNEXUS OLD


#ifdef Q_OS_MAC
    #define QUNEXUS_BL_PORT  "QuNexus Port 1"
    #define QUNEXUS_OLD_IN_P1 "QuNexus Port 1"
    #define QUNEXUS_OLD_IN_P2 "QuNexus Port 2"
    #define QUNEXUS_OLD_IN_P3 "QuNexus Port 3"
    #define QUNEXUS_OLD_OUT_P1 "QuNexus Port 1"
    #define QUNEXUS_OLD_OUT_P2 "QuNexus Port 2"
    #define QUNEXUS_OLD_OUT_P3 "QuNexus Port 3"
#else
    #define QUNEXUS_BL_PORT "QuNexus Control Surface"
    #define QUNEXUS_OLD_IN_P1 "QuNexus"
    #define QUNEXUS_OLD_IN_P2 "MIDIIN2 (QuNexus)"
    #define QUNEXUS_OLD_IN_P3 "MIDIIN3 (QuNexus)"
    #define QUNEXUS_OLD_OUT_P1 "QuNexus"
    #define QUNEXUS_OLD_OUT_P2 "MIDIOUT2 (QuNexus)"
    #define QUNEXUS_OLD_OUT_P3 "MIDIOUT3 (QuNexus)"
#endif

// QUNEXUS NEW

//#ifdef Q_OS_MAC
    #define QUNEXUS_IN_P1 "QuNexus Control Surface"
    #define QUNEXUS_IN_P2 "QuNexus Expander"
    #define QUNEXUS_IN_P3 "QuNexus CV"
    #define QUNEXUS_OUT_P1 "QuNexus Control Surface"
    #define QUNEXUS_OUT_P2 "QuNexus Expander"
    #define QUNEXUS_OUT_P3 "QuNexus CV"
//#else
//    #define QUNEXUS_IN_P1 "QuNexus"
//    #define QUNEXUS_IN_P2 "MIDIIN2 (QuNexus)"
//    #define QUNEXUS_IN_P3 "MIDIIN3 (QuNexus)"
//    #define QUNEXUS_OUT_P1 "QuNexus"
//    #define QUNEXUS_OUT_P2 "MIDIOUT2 (QuNexus)"
//    #define QUNEXUS_OUT_P2 "MIDIOUT3 (QuNexus)"
//#endif
// K-BOARD

#ifdef Q_OS_MAC
    #define KBOARD_IN_P1 "K-Board"
    #define KBOARD_OUT_P1 "K-Board"
#else
    #define KBOARD_IN_P1 "K-Board"
    #define KBOARD_OUT_P1 "K-Board"
#endif

// BOPPAD

#ifdef Q_OS_MAC
    #define BOPPAD_IN_P1 "BopPad"
    #define BOPPAD_OUT_P1 "BopPad"
#else
    #define BOPPAD_IN_P1 "BopPad" // todo: test on windows
    #define BOPPAD_OUT_P1 "BopPad"
#endif

// QuNeo

    #define QUNEO_BL_PORT "QUNEO"
#ifdef Q_OS_MAC
    #define QUNEO_IN_P1 "QuNeo"
    #define QUNEO_OUT_P1 "QuNeo"
#else
    #define QUNEO_IN_P1 "QuNeo"
    #define QUNEO_OUT_P1 "QuNeo"
#endif

// 12 Step1

#ifdef Q_OS_MAC
    #define TWELVESTEP1_IN_P1 "12Step Port 1"
    #define TWELVESTEP1_IN_P2 "12Step Port 2"
    #define TWELVESTEP1_OUT_P1 "12Step Port 1"
    #define TWELVESTEP1_OUT_P2 "12Step Port 2"
#else
    #define TWELVESTEP1_IN_P1 "12Step"
    #define TWELVESTEP1_IN_P2 "MIDIIN2 (12Step)"
    #define TWELVESTEP1_OUT_P1 "12Step"
    #define TWELVESTEP1_OUT_P2 "MIDIOUT2 (12Step)"
#endif

// 12 Step2

#define TWELVESTEP_BL_PORT "12 Step Bootloader"

#ifdef Q_OS_MAC
    #define TWELVESTEP2_IN_P1 "12 Step2 Control Surface"
    #define TWELVESTEP2_OUT_P1 "12 Step2 Control Surface"
    #define TWELVESTEP2_OUT_P2 "12 Step2 TRS MIDI OUT"
#else
    #define TWELVESTEP2_IN_P1 "12 Step2"               // todo: test on windows
    #define TWELVESTEP2_OUT_P1 "12 Step2"
    #define TWELVESTEP2_OUT_P2 "MIDIOUT2 (12 Step2)"
#endif

// SOFTSTEP SSCOM

#ifdef Q_OS_MAC
    #define SS_OLD_IN_P1 "SSCOM Port 1"
    #define SS_OLD_IN_P2 "SSCOM Port 2"
    #define SS_OLD_OUT_P1 "SSCOM Port 1"
    #define SS_OLD_OUT_P2 "SSCOM Port 2"
#else
    #define SS_OLD_IN_P1 "SSCOM"
    #define SS_OLD_IN_P2 "MIDIIN2 (SSCOM)"
    #define SS_OLD_OUT_P1 "SSCOM"
    #define SS_OLD_OUT_P2 "MIDIOUT2 (SSCOM)"
#endif

// SOFTSTEP CURRENT

    #define SS_BL_PORT "SoftStep Bootloader"

    #define SS_IN_P1 "SoftStep Control Surface"
    #define SS_IN_P2 "SoftStep Expander"
    #define SS_OUT_P1 "SoftStep Control Surface"
    #define SS_OUT_P2 "SoftStep Expander"

    #define SS_SHARE_PORT "SoftStep Share"

// K-MIX
    #define KMIX_IN_P1 "Audio Control"
    #define KMIX_IN_P2 "Control Surface"
    #define KMIX_IN_P3 "Expander"
    #define KMIX_OUT_P1 "Audio Control"
    #define KMIX_OUT_P2 "Control Surface"
    #define KMIX_OUT_P3 "Expander"




#endif // KMI_DEV_DATA_H
