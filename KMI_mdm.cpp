// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
/* KMI MIDI Device Manager

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
#include <QMessageBox>

// debugging macro
#define DM_OUT qDebug() << objectName << ": "
#define DM_OUT_P qDebug() << thisMidiDeviceManager->objectName << ": "

MidiDeviceManager::MidiDeviceManager(QWidget *parent, int initPID, QString objectNameInit) :
    QWidget(parent)
{
    // putting these here for now but ideally this should be in a common database/table    
    lookupPID.insert(PID_AUX, "AUX");
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
    objectName = objectNameInit;

#ifdef MDM_DEBUG_ENABLED
    DM_OUT << "MidiDeviceManager created - " << deviceName << " PID:" << PID;
#endif
    port_in = -1;
    port_out = -1;

    // setup RtMidi connections
    midi_in = new RtMidiIn();
    midi_out = new RtMidiOut();

    // open gate
    ioGate = true;

    // flags
    connected = false;
    port_in_open = false;
    port_out_open = false;
    callbackIsSet = false;
    fwUpdateRequested = false;
    globalsRequested = false;

    // firmware and bootloader timeout timer
    //timeoutFwBl = new QTimer(this);

    // timers have to be triggered by these signals from the main thread
    connect(this, SIGNAL(signalStopPolling()), this, SLOT(slotStopPolling()));
    connect(this, SIGNAL(signalBeginBlTimer()), this, SLOT(slotBeginBlTimer()));
    connect(this, SIGNAL(signalBeginFwTimer()), this, SLOT(slotBeginFwTimer()));
    connect(this, SIGNAL(signalStopGlobalTimer()), this, SLOT(slotStopGlobalTimer()));
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
        port_in_open = true;
        return 1;
    }
    port_in_open = false;
    return 0;
}

bool MidiDeviceManager::updatePortOut(int port)
{
    // update and try to re-open
    port_out = port;
    if (slotOpenMidiOut())
    {
        port_out_open = true;
        return 1;
    }
    port_out_open = false;
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
        callbackIsSet = true;

        // Don't ignore sysex, timing, or active sensing messages.
        midi_in->ignoreTypes( false, false, false );
    }
    catch (RtMidiError &error)
    {
        /* Return the error */
        DM_OUT << "OPEN MIDI IN ERR:" << (QString::fromStdString(error.getMessage()));
        return 0;
    }

    if (PID == PID_AUX && !connected) // aux ports don't need firmware to match for connect
    {
        connected = true;
        emit signalConnected(true);
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
    if (PID == PID_AUX && !connected) // aux ports don't need firmware to match for connect
    {
        connected = true;
        emit signalConnected(true);
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
        if (callbackIsSet)
        {
            midi_in->cancelCallback();
            callbackIsSet = false;
        }
    }
    catch (RtMidiError &error)
    {
        /* Return the error */
        DM_OUT << "CLOSE MIDI IN ERR:" << (QString::fromStdString(error.getMessage()));
        return 0;
    }

    // alert host application that we are disconnected
    connected = false;
    emit signalConnected(false);
    bootloaderMode = false;
    port_in_open = false;
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

    // alert host application that we are disconnected
    if (connected) // check so we only emit once
    {
        connected = false;
        emit signalConnected(false);
    }
    bootloaderMode = false;
    port_out_open = false;
    return 1;
}

// sends a sysex universal ack with magic number, if received then alert app
void MidiDeviceManager::slotTestFeedbackLoop()
{
    slotSendSysEx(_sx_ack_loop_test, sizeof(_sx_ack_loop_test));
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
    DM_OUT << "slotStartPolling called";
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

void MidiDeviceManager::slotStopPolling() // this has to be called from an emitted signal so that it's handled in the main thread
{
    versionPoller->stop();
}

void MidiDeviceManager::slotPollVersion()
{
    DM_OUT << "slotPollVersion called - in_open: " << port_in_open << " out_open: " << port_out_open;

    //DM_OUT << "\nslotPollVersion called";

    // ports aren't setup yet
    if (port_in == -1 || port_out == -1 || !port_in_open || !port_out_open)
    {
        if (!fwUpdateRequested) return;

        DM_OUT << "pollTimeout: " << pollTimeout;
        // if we are polling during a firmware update request, try 5 times and then fail
        if (pollTimeout > 5)
        {
            slotFirmwareUpdateReset();
            connected = false;
            emit signalFirmwareUpdateComplete(false);
            slotStopPolling();
            return;
        }
        pollTimeout++;

    }

    if (deviceName == "SoftStep") // softStep doesn't use the universal syx dev id request
    {
        slotSendSysEx(_fw_req_softstep, sizeof(_fw_req_softstep));
    }
    else if (deviceName == "12 Step") // 12 step also doesn't use the universal syx dev id request
    {
        slotSendSysEx(_fw_req_12step, sizeof(_fw_req_12step));
    }
    else // every other product
    {
        slotSendSysEx(_sx_id_req_standard, sizeof(_sx_id_req_standard));
    }
}

void MidiDeviceManager::slotStartGlobalsTimer()
{
    DM_OUT << "slotStartGlobalsTimer called";
    globalsTimerCount = 0;
    timeoutGlobalsReq = new QTimer(this);
    connect(timeoutGlobalsReq, SIGNAL(timeout()), this, SLOT(slotCheckGlobalsReceived()));
    timeoutGlobalsReq->start(1000); // ping every second to see if globals have been received
}

void MidiDeviceManager::slotStopGlobalTimer()
{
    timeoutGlobalsReq->stop(); // stop timer
}

void MidiDeviceManager::slotCheckGlobalsReceived()
{
    DM_OUT << "slotCheckGlobalsReceived called - globalsTimerCount: " << globalsTimerCount;

    if (globalsTimerCount < 5) // under timeout threshold
    {
        emit signalRequestGlobals(); // ping again
        globalsTimerCount++;
    }
    else
    {
        DM_OUT << "globals request timeout";
        emit signalStopGlobalTimer();
        slotFirmwareUpdateReset(); // reset flags
        emit signalFwConsoleMessage("\nERROR: No response to Globals request.");
        emit signalFirmwareUpdateComplete(false); // signal failure
    }
}

// **********************************************************************************
// ***** SysEx slots/functions ******************************************************
// **********************************************************************************

void MidiDeviceManager::slotSendSysExBA(QByteArray thisSysexArray)
{
    unsigned char *ucharArrayPtr;

    // create a uchar pointer to the QByteArray
    ucharArrayPtr = reinterpret_cast<unsigned char*>(thisSysexArray.data());

    slotSendSysEx(ucharArrayPtr, thisSysexArray.size());
}

// takes a pointer and the size of the array
void MidiDeviceManager::slotSendSysEx(unsigned char *sysEx, int len)
{
    DM_OUT << "Send sysex, length: " << len << " syx: " << sysEx << " PID: " << PID << " RtMidi SysEx Packet Size: " << __MACOSX_SYX_XMIT_SIZE__;
    std::vector<unsigned char> message(sysEx, sysEx+len);

    ioGate = false; // pause any midi output while sending SysEx

    try
    {
        midi_out->sendMessage( &message );
    }
    catch (RtMidiError &error)
    {
        DM_OUT << "SYSEX SEND ERR:" << (QString::fromStdString(error.getMessage()));
        emit signalFwConsoleMessage("SYSEX SEND ERR:" + (QString::fromStdString(error.getMessage())));
    }

    ioGate = true; // reopen gate

}

// *************************************************
// slotProcessSysEx - parse incomming sysex, detect
// firmware/id responses, pass along other messages
// appropriate to the device
// *************************************************
void MidiDeviceManager::slotProcessSysEx(QByteArray sysExMessageByteArray, std::vector< unsigned char > *sysExMessageCharArray)
{
    DM_OUT << "slotProcessSysEx called - PID: " << PID << " deviceName: " << deviceName;

    // Test for feedback loop
    QByteArray feedbackLoopTest(reinterpret_cast<char*>(_sx_ack_loop_test), sizeof(_sx_ack_loop_test));
    int feedbackTestIndex = sysExMessageByteArray.indexOf(feedbackLoopTest, 0);

    if (feedbackTestIndex == 0) // feedback loop detected!
    {
        DM_OUT << "*** FEEDBACK LOOP DETECTED, MIDI PORTS CLOSED *** - " << sysExMessageByteArray;
        this->disconnect(SIGNAL(signalRxMidi_raw(uchar, uchar, uchar, uchar)));
        slotCloseMidiIn(); // better than letting the app crash? Only if we alert the end user, otherwise this becomes a support
        slotCloseMidiOut(); // better than letting the app crash? Only if we alert the end user, otherwise this becomes a support
        emit signalFeedbackLoopDetected(this);
        slotErrorPopup("MIDI FEEDBACK LOOP DETECTED\nPorts Closed");
    }

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
        if ((unsigned char)sysExMessageByteArray.at(9) == 1)
        {
            bootloaderMode = true;
        }
        else
        {
            bootloaderMode = false;
        }

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
        if ((unsigned char)sysExMessageByteArray.at(9) == 1)
        {
            bootloaderMode = true;
        }
        else
        {
            bootloaderMode = false;
        }

        devicebootloaderVersion = sysExMessageByteArray.mid(12, 3);
        deviceFirmwareVersion = sysExMessageByteArray.mid(15, 3);

        DM_OUT << "ID Reply - BL: " << devicebootloaderVersion << " FW: " << deviceFirmwareVersion; // << " fullMsg: " << sysExMessageByteArray;
    }

    // process non fw/id SysEx Messages
    else
    {
#ifdef MDM_DEBUG_ENABLED
        DM_OUT << "replyIndex: " << replyIndex;
        DM_OUT << "replyIndex12S: " << replyIndex12S;
        DM_OUT << "fwQueryReply12S: " << fwQueryReply12S;
        DM_OUT << "replyIndexSS: " << replyIndexSS;
        DM_OUT << "reply header: " << fwQueryReply;
        DM_OUT << "Unrecognized Syx: " << QString::fromStdString(sysExMessageByteArray.toStdString());;
#endif

        DM_OUT << "passing SysEx to applicaiton";
        // send SysEx to application
        emit signalRxSysExBA(sysExMessageByteArray);
        emit signalRxSysEx(sysExMessageCharArray);

        // leave function
        return;
    }

    // process firmware version connection messages
    emit signalStopPolling();

    if (bootloaderMode)
    {
        DM_OUT << "Device in bootloader mode";
        emit signalBootloaderMode(fwUpdateRequested);

        // call the firmware update if it was requested
        if (fwUpdateRequested)
        {
            emit signalFwConsoleMessage("Device bootloader detected.\n"); // confirm enter bootloader and next line
            emit signalFwProgress(40); // increment progress bar
            slotUpdateFirmware();
        }
    }
    else if (deviceFirmwareVersion == applicationFirmwareVersion)
    {
        DM_OUT << "emit fw match - fwv: " << deviceFirmwareVersion << "cfwv: " << applicationFirmwareVersion;

        if (fwUpdateRequested) // wait for user to see update completed and click ok
        {
            if (globalsRequested)
            {
                globalsRequested = false;
                emit signalRestoreGlobals();
                emit signalFwProgress(90); // increment progress bar
                signalFwConsoleMessage("\nRestoring Globals...");
            }
            emit signalFirmwareUpdateComplete(true);
        }
        else
        {
            emit signalFirmwareDetected(this, true);
            emit signalConnected(true);
            connected = true;
        }

    }
    else
    {
        DM_OUT << "emit fw mismatch - fwv: " << deviceFirmwareVersion << "cfwv: " << applicationFirmwareVersion;
        emit signalFirmwareDetected(this, false);
        //emit signalFirmwareMismatch(QString(devicebootloaderVersion), QString(applicationFirmwareVersion), QString(deviceFirmwareVersion));
    }

}

void MidiDeviceManager::slotOpenFirmwareFile(QString filePath)
{
    //Load Firmware File into a byte array
    firmware = new QFile(filePath);
    firmware->open(QIODevice::ReadOnly);
    firmwareByteArray = firmware->readAll();
}

// set up the firmware update process - enter bootloader, wait, send ud
void MidiDeviceManager::slotRequestFirmwareUpdate()
{
    DM_OUT << "slotRequestFirmwareUpdate - fwUpdteRequested: " << fwUpdateRequested << " bootloaderMode: " << bootloaderMode << " globalsRequested: " << globalsRequested;
    fwUpdateRequested = true;

    if (bootloaderMode)
    {
        DM_OUT << "slotupdatefirmware";
        slotUpdateFirmware();
    }
    else
    {
        // handle devices with global settings to back up before updating fw
        if (PID == PID_QUNEXUS)
        {
            if (globalsRequested) // this flag is set if we've received globals and called slotReQuestFirmwareUpdate again
            {
                DM_OUT << "Globals received, stop timer and enter bootloader";
                emit signalStopGlobalTimer();
                emit signalStopPolling();

                emit signalFwConsoleMessage("\nGlobals Saved.");
                emit signalFwProgress(25); // increment progress bar
                slotEnterBootloader();  // enter bootloader, which will start firmware request
            }
            else
            {
                DM_OUT << "Begin globals backup - send request and start timer";
                globalsRequested = true; // set flag, it gets cleared when firmware completes or times out
                emit signalRequestGlobals();
                slotStartGlobalsTimer(); // start the timer
                emit signalFwProgress(10); // increment progress bar
                signalFwConsoleMessage(QString("\nBacking up %1 global settings...").arg(deviceName));
            }
        }
        // handle devices that don't have anything to back up before fw update
        else
        {
            emit signalFwProgress(25); // increment progress bar
            slotEnterBootloader();  // enter bootloader, which will start firmware request
        }
    }
}

void MidiDeviceManager::slotEnterBootloader()
{
    signalFwConsoleMessage("\nSending Enter bootloader Command, device will reboot.\n");
    signalFwProgress(20); // increment progress bar
    // TODO - this works for qunexus, add code for other controllers
    switch (PID)
    {
    case PID_QUNEXUS:
        slotSendSysEx(_bl_qunexus, sizeof(_bl_qunexus));
        break;
    case PID_QUNEO:
        slotSendSysEx(_bl_quneo, sizeof(_bl_quneo));
        break;
    default:
        DM_OUT << "bootloader command not configured for this device";
    }

    DM_OUT << "BL timeout counter started";
    emit signalBeginBlTimer();
}

void MidiDeviceManager::slotBeginBlTimer()
{
    DM_OUT << "slotBeginBlTimer called";
    QTimer::singleShot(3000, this, SLOT(slotBootloaderTimeout()));
}

void MidiDeviceManager::slotBootloaderTimeout()
{
    //delete timeoutFwBl;
    DM_OUT << "slotBootloaderTimeout called - bootloaderMode: " << bootloaderMode;
    if (bootloaderMode) return;
    emit signalFwConsoleMessage(QString("\nPinging %1 for bootloader status...\n").arg(deviceName));
    slotStartPolling(); // begin polling
}

void MidiDeviceManager::slotUpdateFirmware()
{
    DM_OUT << "slotUpdateFirmware called - port_out_open: " << port_out_open << " fw .syx size: " << firmwareByteArray.length();
    if (port_out_open == false)
    {
#ifndef Q_OS_WIN
        emit signalFwConsoleMessage(QString("\%1 not connected!\n").arg(deviceName));
#else
        emit signalFwConsoleMessage(QString("\%1 MIDI driver not connected or unavailable.\n"
                               "Windows cannot share standard USB MIDI drivers, try\n"
                               "closing all programs, re-starting the %1 editor,\n"
                               "and then reconnecting your %1.\n").arg(deviceName));
#endif
        return;
    }
    emit signalFwConsoleMessage("\nUpdating Firmware...\n");
    emit signalFwProgress(50); // increment progress bar

    if (firmwareByteArray.length() < 1)
    {
        DM_OUT << "Firmware file not defined!";
        emit signalFwConsoleMessage("ERROR! Firmware file not found!");
        return; // no file, should trip an error
    }

    // reworked for big sur, need to send in blocks rather than one large packet
    DM_OUT << "sending firmware sysex";

//    int blockSize = 10000; // break the sysex file into blocks this big
//    int waitTime = 500; // wait this ammount of time between sending blocks
//    int blockNumber = 0;
//    QByteArray thisPayload = firmwareByteArray; // temporarily store payload
//    QElapsedTimer blockTimer;

//    while (thisPayload.length() > 0)
//    {
//        if (thisPayload.length() > blockSize)
//        {
//            DM_OUT << "Sending block #: " << blockNumber++ << " size: " << blockSize;
//            slotSendSysExBA(thisPayload.left(blockSize)); // send a block
//            // remove block from packet
//            thisPayload = thisPayload.mid(blockSize + 1, // start at the byte after this block
//                                        thisPayload.length() - blockSize); // the remainder of the packet
//            DM_OUT << "Remaining payload size: " <<  thisPayload.length();
//            DM_OUT << "";
//        }
//        else
//        {
//            DM_OUT << "Sending final block #: " << blockNumber++ << " size: " << thisPayload.length();
//            slotSendSysExBA(thisPayload); // send the remainder of the packet
//            thisPayload = ""; // clear the packet
//        }

//        DM_OUT << "Wait " << waitTime << "ms";
//        DM_OUT << "";
//        blockTimer.start();
//        while (blockTimer.elapsed() < waitTime)
//        {
//            continue;
//        }

//    }

    slotSendSysExBA(firmwareByteArray); // send the remainder of the packet

    DM_OUT << "FW timeout counter started";
    emit signalBeginFwTimer();
}

void MidiDeviceManager::slotBeginFwTimer()
{
    DM_OUT << "slotBeginFwTimer called";
    QTimer::singleShot(15000, this, SLOT(slotFirmwareTimeout()));
}

void MidiDeviceManager::slotFirmwareTimeout()
{
    //delete timeoutFwBl;
    DM_OUT << "slotFirmwareTimeout called - fwUpdateRequested: " << fwUpdateRequested;
    if (!fwUpdateRequested) return;

    emit signalFwProgress(75); // increment progress bar
    emit signalFwConsoleMessage("\nPinging Device for version info...\n");
    slotStartPolling(); // begin polling
}

void MidiDeviceManager::slotFirmwareUpdateReset()
{
    DM_OUT << "slotFirmwareUpdateReset called";
    fwUpdateRequested = false;
    bootloaderMode = false;
    globalsRequested = false;
}


// **********************************************************************************
// ***** Channel and system common slots/functions **********************************
// **********************************************************************************

// overload
void MidiDeviceManager::slotSendMIDI(uchar status)
{
    slotSendMIDI(status, 255, 255, 255);
}
void MidiDeviceManager::slotSendMIDI(uchar status, uchar d1)
{
    slotSendMIDI(status, d1, 255, 255);
}
void MidiDeviceManager::slotSendMIDI(uchar status, uchar d1, uchar d2)
{
    slotSendMIDI(status, d1, d2, 255);
}

// Send a MIDI message. Handles 1/2/3 byte packets. Chan goes last to allow 2/3 byte system common messages to omit channel
void MidiDeviceManager::slotSendMIDI(uchar status, uchar d1 = 255, uchar d2 = 255, uchar chan = 255)
{
    //DM_OUT << QString("slotSendMIDI called - status: %1 d1: %2 d2: %3 channel: %4").arg(status).arg(d1).arg(d2).arg(chan);
    uchar newStatus;
    std::vector<uchar> packet;
    packet.clear();

    if (ioGate == false)
    {
        DM_OUT << "ioGate stopped an incomming MIDI message - status:" << status;
        return; // do not send MIDI while sysex is transmitting
    }

    switch (status)
    {
    // **********************************
    // ******** CHANNEL MESSAGES ********
    // **********************************

    // three byte packets
    case MIDI_NOTE_OFF:
    case MIDI_NOTE_ON:
    case MIDI_NOTE_AFTERTOUCH:
    case MIDI_CONTROL_CHANGE:
    case MIDI_PITCH_BEND:
        if (chan > 127 || d1 > 127 || d2 > 127) return; // catch bad data
        newStatus = (status + chan);
        packet.push_back(newStatus);
        packet.push_back(d1);
        packet.push_back(d2);
        break;
    // two byte packets
    case MIDI_PROG_CHANGE:
    case MIDI_CHANNEL_PRESSURE:
        if (chan > 127 || d1 > 127) return; // catch bad data
        newStatus = (status + chan);
        packet.push_back(newStatus);
        packet.push_back(d1);
        break;

    // **********************************
    // ****** SYS COMMON MESSAGES *******
    // **********************************

    // three byte packets
    case MIDI_MTC:
    case MIDI_SONG_POSITION:
        if (d1 > 127 || d2 > 127) return; // catch bad data
        packet.push_back(status);
        packet.push_back(d1);
        packet.push_back(d2);
        break;
    // two byte packets
    case MIDI_SONG_SELECT:
        if (d1 > 127) return; // catch bad data
        packet.push_back(status);
        packet.push_back(d1);
        break;
    // single byte packets
    case MIDI_TUNE_REQUEST:
    case MIDI_RT_CLOCK:
    case MIDI_RT_START:
    case MIDI_RT_CONTINUE:
    case MIDI_RT_STOP:
    case MIDI_RT_ACTIVE_SENSE:
    case MIDI_RT_RESET:
        packet.push_back(status);
        break;
    // catch undefined and bad messages
    default:
        return; // go no further
        break;
    }

#ifdef MDM_DEBUG_ENABLED
    DM_OUT << "Send MIDI - packet: " << packet;
#endif

    // prepare and send the packet
    //std::vector<unsigned char> message(packet, packet+len);
    midi_out->sendMessage( &packet );
}

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

#ifdef MDM_DEBUG_ENABLED
    DM_OUT << "Packet Received - status: " << status << " ch: " << chan << " d1: " << data1 << " d2: " << data2;
#endif

    // emit raw packet, makes direct routing between ports simple
    emit signalRxMidi_raw(status, data1, data2, chan);

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
// ***** Error Popup ****************************************************************
// **********************************************************************************
void MidiDeviceManager::slotErrorPopup(QString errorMessage)
{
    QMessageBox errBox;
    errBox.setText(errorMessage);
    errBox.exec();
}

// **********************************************************************************
// ***** Callback *******************************************************************
// **********************************************************************************

void MidiDeviceManager::midiInCallback( double deltatime, std::vector< unsigned char > *message, void *thisCaller )
{
    // create pointer for the mdm that called this
    MidiDeviceManager * thisMidiDeviceManager;
    thisMidiDeviceManager = (MidiDeviceManager*) thisCaller;

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
    else
    {
        DM_OUT_P << "ioGate stopped midiInCallback - message size:" << message->size();
        return; // do not send MIDI while sysex is transmitting
    }
}
