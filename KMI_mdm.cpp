// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
/* KMI Device Manager

  A cross-platform C++/Qt MIDI library for KMI devices.
  Written by Eric Bateman, August 2021.
  (c) Copyright 2021 Keith McMillen Instruments, all rights reserved.

  Features:
  - Handles connectivity to KMI MIDI devices
  - Detects firmware version, reports match/mismatch
  - Proivdes method to update firmware
  - Parses channel, system common, and sysex data from device and passes signals to main application
  - Provides methods to transmit channel, system common, and sysex data to device

  Todo:
  - Encode/Decode 8-bit SysEx and CRC

  Uses KMI_Ports and RtMidi

  Additional resources:
  - KMI_FwVersions.h - contains all current fw versions for KMI products
  - KMI_SysexMessages.h - contains common SysEx strings used with KMI products (ie device specific fw request/response)

*/

#include "KMI_mdm.h"
#include "KMI_DevData.h"
#include "KMI_SysexMessages.h"

// debugging macro
#define DM_OUT qDebug() << deviceName << ": "
#define DM_OUT_P qDebug() << thisMidiDeviceManager->deviceName << ": "

MidiDeviceManager::MidiDeviceManager(QWidget *parent, int initPID) :
    QWidget(parent)
{
    // putting these here for now but ideally this should be in a common database/table    
    lookupPID.insert(PID_THRU, "THRU");
    lookupPID.insert(PID_STRINGPORT, "StringPort");
    lookupPID.insert(PID_SOFTSTEP, "SoftStep");
    lookupPID.insert(PID_12STEP, "12 Step");
    lookupPID.insert(PID_QUNEXUS, "QuNexus");
    lookupPID.insert(PID_KBOARD, "K-Board");
    lookupPID.insert(PID_APPL_CBL, "KMI Apple Cable");
    lookupPID.insert(PID_QUNEO, "QuNeo");
    lookupPID.insert(PID_ROGUE, "Rogue");
    lookupPID.insert(PID_KMIX, "K-Mix");
    lookupPID.insert(PID_KMIX_CTL, "K-Mix Control");
    lookupPID.insert(PID_KBP4, "K-Board Pro 4");
    lookupPID.insert(PID_KBP4_BL, "KBP4 Bootloader");
    lookupPID.insert(PID_EM1, "MalletStation");
    lookupPID.insert(PID_EM1_BL, "MalletStation Bootloader");
    lookupPID.insert(PID_BOPPAD, "BopPad");
    lookupPID.insert(PID_BOPPAD_BL, "BopPad Bootloader");


    PID = initPID;
    deviceName = lookupPID.value(PID);

    DM_OUT << "MidiDeviceManager created - " << deviceName << " PID:" << PID;
    port_in = -1;
    port_out = -1;

    // setup RtMidi connections
    midi_in = new RtMidiIn();
    midi_out = new RtMidiOut();

    ioGate = true;
    fwUpdateRequested = false;

    versionReply = false;
    queryReplied = false;

    connect(this, SIGNAL(signalStopPolling()), this, SLOT(slotStopPolling()));
}

// **********************************************************************************
// ****** Port slots/functions ******************************************************
// **********************************************************************************

bool MidiDeviceManager::updatePortIn(int port)
{
    // update and try to re-open
    port_in = port;
    if (slotOpenMidiIn())
    {
        return 1;
    }
    return 0;
}

bool MidiDeviceManager::updatePortOut(int port)
{
    // update and try to re-open
    port_out = port;
    if (slotOpenMidiOut())
    {
        return 1;
    }
    return 0;
}

bool MidiDeviceManager::slotOpenMidiIn()
{
    //DM_OUT << "slotOpenMidiIn called - port: " << port_in;

    try
    {
        // first close the port to avoid errors
        if (!slotCloseMidiIn()) DM_OUT << "couldn't close in port: " << port_in;

        //open ports
        midi_in->openPort(port_in);

        // setup callback
        midi_in->setCallback( &MidiDeviceManager::midiInCallback, this);

        // Don't ignore sysex, timing, or active sensing messages.
        midi_in->ignoreTypes( false, false, false );
    }
    catch (RtMidiError &error)
    {
        /* Return the error */
        DM_OUT << "OPEN MIDI IN ERR:" << (QString::fromStdString(error.getMessage()));
        return 0;
    }
    return 1;
}

bool MidiDeviceManager::slotOpenMidiOut()
{
    //DM_OUT << "slotOpenMidiOut called - port: " << port_out;

    try
    {
        // first close the port to avoid errors
        if (!slotCloseMidiOut()) DM_OUT << "couldn't close out port: " << port_out;

        //open ports
        midi_out->openPort(port_out);
    }
    catch (RtMidiError &error)
    {
        /* Return the error */
        DM_OUT << "OPEN MIDI OUT ERR:" << (QString::fromStdString(error.getMessage()));
        return 0;
    }
    return 1;
}

bool MidiDeviceManager::slotCloseMidiIn()
{
    //DM_OUT << "slotCloseMidiIn called";

    try
    {
        //close ports
        midi_in->closePort();
    }
    catch (RtMidiError &error)
    {
        /* Return the error */
        DM_OUT << "CLOSE MIDI IN ERR:" << (QString::fromStdString(error.getMessage()));
        return 0;
    }
    return 1;
}

bool MidiDeviceManager::slotCloseMidiOut()
{
    //DM_OUT << "slotCloseMidiOut called";

    try
    {
        //close ports
        midi_out->closePort();
    }
    catch (RtMidiError &error)
    {
        /* Return the error */
        DM_OUT << "CLOSE MIDI OUT ERR:" << (QString::fromStdString(error.getMessage()));
        return 0;
    }
    return 1;
}

// **********************************************************************************
// ****** firmware polling **********************************************************
// **********************************************************************************

void MidiDeviceManager::slotSetExpectedFW(QByteArray fwVer)
{
    applicationFirmwareVersion = fwVer;
    //DM_OUT << "Set curFwVer: " << currentFwVer;
}

// timer to regularly ping the device for firmware info and to confirm connection
void MidiDeviceManager::slotStartPolling()
{
    int pollTime;

    versionPoller = new QTimer(this);
    connect(versionPoller, SIGNAL(timeout()), this, SLOT(slotPollVersion()));

    // some devices take longer to boot up
    if (deviceName == "12 Step" ||
        deviceName == "SoftStep")
    {
        pollTime = 2000;
    }
    else
    {
        pollTime = 500;
    }
    versionPoller->start(pollTime);
}

void MidiDeviceManager::slotStopPolling()
{
    versionPoller->stop();
}

void MidiDeviceManager::slotPollVersion()
{
    //DM_OUT << "\nslotPollVersion called";

    // ports aren't setup yet
    if (port_in == -1 || port_out == -1) return;

    if(!queryReplied)
    {
        if (deviceName == "SoftStep") // softStep doesn't use the universal syx dev id request
        {
            slotSendSysEx(_fw_req_softstep, sizeof(_fw_req_softstep));
        }
        else if (deviceName == "12 Step") // 12 step also doesn't use the universal syx dev id request
        {
            slotSendSysEx(_fw_req_12step, sizeof(_fw_req_12step));
        }
        else // every other product does (EB TODO - verify this with 12 step)
        {
            slotSendSysEx(_sx_id_req_standard, sizeof(_sx_id_req_standard));
        }
    }
    else
    {
        DM_OUT << "STOP";

        emit signalConnected(true);

    }
}

// **********************************************************************************
// ***** SysEx slots/functions ******************************************************
// **********************************************************************************

void MidiDeviceManager::slotSendSysEx(unsigned char *sysEx, int len)
{
    DM_OUT << "Send sysex, length: " << len << " syx: " << sysEx << " PID: " << PID;
    std::vector<unsigned char> message(sysEx, sysEx+len);
    midi_out->sendMessage( &message );

}

// *************************************************
// slotProcessSysEx - parse incomming sysex, detect
// firmware/id responses, pass along other messages
// appropriate to the device
// *************************************************
void MidiDeviceManager::slotProcessSysEx(QByteArray sysExMessageByteArray, std::vector< unsigned char > *sysExMessageCharArray)
{
    // DM_OUT << "slotProcessSysEx called - PID: " << PID << " deviceName: " << deviceName;

    // Query for SoftStep
    QByteArray fwQueryReplySS(reinterpret_cast<char*>(_fw_reply_softstep), sizeof(_fw_reply_softstep));
    int replyIndexSS = sysExMessageByteArray.indexOf(fwQueryReplySS, 0);

    // Query for 12 Step
    QByteArray fwQueryReply12S(reinterpret_cast<char*>(_fw_reply_12step), sizeof(_fw_reply_12step));
    int replyIndex12S = sysExMessageByteArray.indexOf(fwQueryReply12S, 0);

    // Query for all other devices (test 12Step)
    QByteArray fwQueryReply(reinterpret_cast<char*>(_sx_id_reply_standard), sizeof(_sx_id_reply_standard));
    int replyIndex = sysExMessageByteArray.indexOf(fwQueryReply, 0);

    // ***** Soft Step **************************************
    if (replyIndexSS == 2)
    {
        DM_OUT << "SoftStep fw reply:" <<  sysExMessageByteArray;
        deviceFirmwareVersion = sysExMessageByteArray.mid(68, 1);
        DM_OUT << "SoftStep fw ver: " << (uchar)sysExMessageCharArray->at(68);
    }

    // ***** 12 Step ****************************************
    else if (replyIndex12S == 1)
    {
        DM_OUT << "12Step fw reply - version: " << (uchar)sysExMessageCharArray->at(68);
        deviceFirmwareVersion = sysExMessageByteArray.mid(68, 1);
    }

    // ***** QuNeo ****************************************
    else if (replyIndex == 0 && deviceName == "QuNeo")
    {
        // reset
        devicebootloaderVersion.clear();
        deviceFirmwareVersion.clear();

        // get bl ver
        devicebootloaderVersion.append ( ((uchar)sysExMessageCharArray->at(13)) ); // MSB
        devicebootloaderVersion.append ( ((uchar)sysExMessageCharArray->at(12)) ); // LSB

        // get fw ver
        deviceFirmwareVersion.append( ((uchar)sysExMessageCharArray->at(15) & 0xF0) >> 4 ); // left 4 bits
        deviceFirmwareVersion.append( ((uchar)sysExMessageCharArray->at(15) & 0x0F) );      // right 4 bits
        deviceFirmwareVersion.append( ((uchar)sysExMessageCharArray->at(14)) ); // LSB

        DM_OUT << "QuNeo fw reply- BL: " << devicebootloaderVersion << " FW: " << deviceFirmwareVersion << " fullMsg: " << sysExMessageByteArray;
    }
    // ***** All others *************************************
    else if (replyIndex == 0)
    {
        devicebootloaderVersion = sysExMessageByteArray.mid(12, 3);
        deviceFirmwareVersion = sysExMessageByteArray.mid(15, 3);

        DM_OUT << "ID Reply - BL: " << devicebootloaderVersion << " FW: " << deviceFirmwareVersion << " fullMsg: " << sysExMessageByteArray;
    }

    // process non fw/id SysEx Messages
    else
    {
        DM_OUT << "replyIndex: " << replyIndex;
        DM_OUT << "replyIndex12S: " << replyIndex12S;
        DM_OUT << "fwQueryReply12S: " << fwQueryReply12S;
        DM_OUT << "replyIndexSS: " << replyIndexSS;
        DM_OUT << "reply header: " << fwQueryReply;
        DM_OUT << "Unrecognized Syx: " << QString::fromStdString(sysExMessageByteArray.toStdString());;

        // send SysEx to application
        emit signalRxSysEx(sysExMessageByteArray, sysExMessageCharArray);

        // leave function
        return;
    }

    // process firmware connection messages
    emit signalStopPolling();
    emit signalConnected(true);

    if (deviceFirmwareVersion == applicationFirmwareVersion)
    {
        DM_OUT << "emit fw match - fwv: " << deviceFirmwareVersion << "cfwv: " << applicationFirmwareVersion;
        emit signalFirmwareMatches(this);
    }
    else
    {
        DM_OUT << "emit fw mismatch - fwv: " << deviceFirmwareVersion << "cfwv: " << applicationFirmwareVersion;
        emit signalFirmwareMismatch(this);
    }
}

void MidiDeviceManager::slotUpdateFirmware()
{
    DM_OUT << "empty slotUpdateFirmware called ";
}


// **********************************************************************************
// ***** Channel and system common slots/functions **********************************
// **********************************************************************************

void MidiDeviceManager::slotParsePacket(QByteArray packetArray)
{
    // keep static for running status
    static unsigned char status, chan;

    unsigned char data1, data2;

    if (uchar(packetArray[0]) > 127) // not running status
    {
        status = packetArray[0] & 0xF0;
        chan = packetArray[0] & 0x0F;
        // check packet length
        data1 = (packetArray.length() > 1) ? packetArray[1] : 0;
        data2 = (packetArray.length() > 2) ? packetArray[2] : 0;
    }
    else // running status
    {
        DM_OUT << "running status!";
        data1 = packetArray[0];
        data2 = packetArray[1];
    }

    DM_OUT << "Packet Received - status: " << status << " ch: " << chan << " d1: " << data1 << " d2: " << data2;

    // handle channel messages
    switch(status)
    {
    case MIDI_NOTE_OFF:
        emit signalRxMidi_noteOff(chan, data1, data2);
        break;

    case MIDI_NOTE_ON: // 0x09
        emit signalRxMidi_noteOn(chan, data1, data2);
        break;

    case MIDI_NOTE_AFTERTOUCH: // 0xA0
        emit signalRxMidi_polyAT(chan, data1, data2);
        break;

    case MIDI_CONTROL_CHANGE:
        unsigned cc, val;
        cc = data1;
        val = data2;

        // RPNs are a 14bit address (CC100 and CC101) with a 14bit data value (CC6 and CC38). In practice
        // the midi org has only registered 6 addresses, and the MPE Configuration Message sets channels, so
        // only the MSB (CC6) is used for data entry. The below routine preserves compatability and could be
        // used as a basis for supporting NRPNs, which are manufacture defined. There is some confusion in the
        // industry about DATA_INC and DATA_DEC as to wether they adjust DATA_MSB or DATA_LSB. For KBP4 we will
        // ignore LSB, INC and DEC. -EB
        uchar *param_MSB;
        uchar *param_LSB;
        int paramNum;

        if (RPNorNRPN == RPN)
        {
            param_MSB = &RPN_MSB[chan];
            param_LSB = &RPN_LSB[chan];
            paramNum = (*param_MSB << 7) | *param_LSB;
        }
        else // NRPN
        {
            param_MSB = &NRPN_MSB[chan];
            param_LSB = &NRPN_LSB[chan];
            paramNum = (*param_MSB << 7) | *param_LSB;
        }

        switch(cc)
        {
        case MIDI_CC_NRPN_LSB:
            *param_LSB = val;
            RPNorNRPN = NRPN;
            break;
        case MIDI_CC_NRPN_MSB:
            *param_MSB = val;
            RPNorNRPN = RPN;
            break;
        case MIDI_CC_RPN_LSB:
            *param_LSB = val;
            RPNorNRPN = NRPN;
            break;
        case MIDI_CC_RPN_MSB:
            *param_MSB = val;
            RPNorNRPN = RPN;
            break;
        case MIDI_CC_DATA_MSB:
            slotRxParam(paramNum, val, chan, DATA_MSB); // set data MSB
            break;
        case MIDI_CC_DATA_LSB:
            slotRxParam(paramNum, val, chan, DATA_LSB); // set data LSB
            break;
        case MIDI_CC_DATA_INC:
            slotRxParam(paramNum, val, chan, DATA_INC); // should be +1, ignore val
            break;
        case MIDI_CC_DATA_DEC:
            slotRxParam(paramNum, val, chan, DATA_DEC); // should be -1, ignore val
            break;
        // end reserved CCs
        default:
            emit signalRxMidi_controlChange(chan, cc, val);
            break;
        }
        break; // end CCs

    case MIDI_PROG_CHANGE:
        emit signalRxMidi_progChange(chan, data1);
        break;

    case MIDI_CHANNEL_PRESSURE:
        emit signalRxMidi_aftertouch(chan, data1);
        break;

    case MIDI_PITCH_BEND:
        emit signalRxMidi_pitchBend(chan, (data1 << 7) | data2);
        break;

    // handle system common messages

    case MIDI_MTC:
        emit signalRxMidi_MTC(data1, data2);
        break;
    case MIDI_SONG_POSITION:
        emit signalRxMidi_SongPosition(data1, data2);
        break;
    case MIDI_SONG_SELECT:
        emit signalRxMidi_SongSelect(data1);
        break;
    case MIDI_TUNE_REQUEST:
        emit signalRxMidi_TuneReq();
        break;
    case MIDI_RT_CLOCK:
        emit signalRxMidi_Clock();
        break;
    case MIDI_RT_START:
        emit signalRxMidi_Start();
        break;
    case MIDI_RT_CONTINUE:
        emit signalRxMidi_Continue();
        break;
    case MIDI_RT_STOP:
        emit signalRxMidi_Stop();
        break;
    case MIDI_RT_ACTIVE_SENSE:
        emit signalRxMidi_ActSense();
        break;
    case MIDI_RT_RESET:
        emit signalRxMidi_SysReset();
        break;
    }
}

void MidiDeviceManager::slotRxParam(int param, uchar val, uchar chan, uchar messagetype)
{
    if (RPNorNRPN == RPN)
    {
        emit signalRxMidi_RPN(chan, param, val, messagetype);
    }
    else
    {
        emit signalRxMidi_NRPN(chan, param, val, messagetype);
    }
}

// **********************************************************************************
// ***** Callback *******************************************************************
// **********************************************************************************

void MidiDeviceManager::midiInCallback( double deltatime, std::vector< unsigned char > *message, void *thisCaller )
{
    // create pointer for the mdm that called this
    MidiDeviceManager * thisMidiDeviceManager;
    thisMidiDeviceManager = (MidiDeviceManager*) thisCaller;

    // check timing of events, might not be necessary
    static double lastDeltatime;

    if (deltatime < lastDeltatime)
    {
        DM_OUT_P << "MIDI events received out of order - last: " << lastDeltatime << " this:" << deltatime;
    }
    lastDeltatime = deltatime;

    DM_OUT_P << "midi in - time: " << deltatime << "length: " << message->size();

    QByteArray packetArray;

    // ioGate used to pause MIDI during sysex...?
    if(thisMidiDeviceManager->ioGate)
    {
        if (message->size() < 999)
        {
            // load the packet into our QByteArray
            for (int i = 0; i < (int)message->size(); i++)
            {
                packetArray.append( (uchar)message->at(i) );
            }

            //DM_OUT_P << "midi in - message size: " << message->size() << " byte0: " << (uchar)packetArray[0];

            // standard messages
            if ((uchar)packetArray[0] != (uchar)MIDI_SX_START)
            {
                DM_OUT_P << "MIDI Channel Event: ";
                thisMidiDeviceManager->slotParsePacket(packetArray);
            }
            else // sysex
            {
                DM_OUT_P << "SysEx received";
                thisMidiDeviceManager->slotProcessSysEx(packetArray, message);
            }
        }
        else
        {
            DM_OUT_P << "ERROR- MIDI Message greater than 999 bytes (" << message->size() << " bytes) - write some more code to handle this!!";
        }
    }
}

