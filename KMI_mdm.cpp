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

//#define MDM_DEBUG_ENABLED 1

// debugging macro
#define DM_OUT qDebug() << deviceName << ": "
#define DM_OUT_P qDebug() << thisMidiDeviceManager->objectName << ": "

MidiDeviceManager::MidiDeviceManager(QWidget *parent, int initPID, QString objectNameInit, KMI_Ports *kmiP) :
    QWidget(parent)
{
    kmiPorts = kmiP; // store this now

    // putting these here for now but ideally this should be in a common database/table    
    lookupPID.insert(PID_AUX, "AUX");
    lookupPID.insert(PID_STRINGPORT, "StringPort");
    lookupPID.insert(PID_SOFTSTEP1, "SoftStep1");
    lookupPID.insert(PID_SOFTSTEP2, "SoftStep2");
    lookupPID.insert(PID_SOFTSTEP_BL, "SoftStep Bootloader");
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

    slotUpdatePID(initPID); // updates PID and deviceName
    objectName = objectNameInit; // unmodifiable

#ifdef MDM_DEBUG_ENABLED
    DM_OUT << "MidiDeviceManager created - " << deviceName << " PID:" << PID;
#endif

    // initialize variables
    PID_MIDI = 0;

    port_in = -1;
    port_out = -1;

    applicationFirmwareVersion.resize(3);
    applicationFirmwareVersion[0] = 0;
    applicationFirmwareVersion[1] = 0;
    applicationFirmwareVersion[2] = 0;

    deviceFirmwareVersion.resize(3);
    deviceFirmwareVersion[0] = 0;
    deviceFirmwareVersion[1] = 0;
    deviceFirmwareVersion[2] = 0;

    devicebootloaderVersion.resize(3);
    devicebootloaderVersion[0] = 0;
    devicebootloaderVersion[1] = 0;
    devicebootloaderVersion[2] = 0;

    firmwareByteArray.resize(0);
    bootloaderByteArray.resize(0);

//    globalsTimerCount = 0;
//    pollTimeout = 0;


    // flags
    connected = false;
    restart = false;
    ioGate = true; // open gate
    port_in_open = false;
    port_out_open = false;
    callbackIsSet = false;

//    firstFwResponseReceived = false;

    fwSaveRestoreGlobals = false;

//    globalsRequested = false;
    bootloaderMode = false;
//    fwUpdateRequested = false;
//    hackStopTimer = false;
    pollingStatus = false;
//    failFlag = false;

    // let's replace all of these flags with a single state variable
    firmwareUpdateState = FWUD_STATE_IDLE;
    firmwareUpdateStateTimer.start(); // a timer to track elapsed ms since last state change
    fwVerPollSkipConnectCycles = 0;
    firstFwVerRequestHasBeenSent = false;
    fwVerRequestTimer.start();

    //DM_OUT << "fwVerPollSkipConnectCycles = 0";

    // setup RtMidi connections
    midi_in = new RtMidiIn();
    midi_out = new RtMidiOut();
    //midi_in = nullptr;
    //midi_out = nullptr;

    // firmware and bootloader timeout timer
    //timeoutFwBl = new QTimer(this);

    // this will become the heartbeat of our polling/firmware update process
    versionPoller = new QTimer(this);
    versionPoller->start(1000); // start the timer

//    versionReplyTimer.start();
//    refreshTimer.start();
//    delayFwTimer.start();

    // timers have to be triggered by these signals from the main thread
    //connect(this, SIGNAL(signalStartPolling(QString)), this, SLOT(slotStartPolling(QString)));
    //connect(this, SIGNAL(signalStopPolling(QString)), this, SLOT(slotStopPolling(QString)));
//    connect(this, SIGNAL(signalBeginBlTimer()), this, SLOT(slotBeginBlTimer()));
//    connect(this, SIGNAL(signalBeginFwTimer()), this, SLOT(slotBeginFwTimer()));
//    connect(this, SIGNAL(signalStopGlobalTimer()), this, SLOT(slotStopGlobalTimer()));

    if (PID != PID_AUX)
    {
        slotStartPolling(objectName);
    }
}

void MidiDeviceManager::slotUpdatePID(int thisPID)
{
    PID = thisPID;
    deviceName = lookupPID.value(PID);
    qDebug() << "updatePID: " << PID << " deviceName: " << deviceName;
}

// **********************************************************************************
// ****** Port slots/functions ******************************************************
// **********************************************************************************

bool MidiDeviceManager::slotUpdatePortIn(int port)
{
    DM_OUT << "updatePortIn called";
    // update and try to re-open
    port_in = port;

    if (slotOpenMidiIn())
    {
        return 1;
    }
    slotCloseMidiIn(SIGNAL_SEND); // close the port if we failed
    return 0;
}

bool MidiDeviceManager::slotUpdatePortOut(int port)
{
    DM_OUT << "updatePortOut called";
    // update and try to re-open
    port_out = port;
    if (slotOpenMidiOut())
    {
        return 1;
    }
    slotCloseMidiOut(SIGNAL_SEND); // close the port if we failed
    return 0;
}

QByteArray MidiDeviceManager::decode8BitArray(QByteArray this8BitArray)
{
    DM_OUT << "decode8BitArray called";
    int         numPackets = ceil(double(this8BitArray.size() / 8));
    int         counter = 0;
    uchar       buffer[8];
    QByteArray  decodedArray;

    for(int thisPacket = 0; thisPacket < numPackets; thisPacket++)
    {
        // fill buffer with 8 bytes from array
        for(uchar thisByte = 0; thisByte < 8; thisByte++)
        {
            int thisIndex = (thisByte + (thisPacket * 8));
            if (thisIndex < this8BitArray.size())
            {
                buffer[thisByte] = this8BitArray.at(thisIndex);
            }
            else
            {
                buffer[thisByte] = 0; //
                DM_OUT << "7bit to 8bit array conversion - last packet is truncated - Packet: " << thisPacket << "Byte: " << thisByte << " index: " << thisIndex;
            }
        }

        //Decode packet
        for(uchar thisByte = 0; thisByte < 7; thisByte++)
        {
            //If decode byte bit 0 is high
            if(buffer[7] & 0x01)
            {
                buffer[thisByte] |= 0x80; // set bit 7 high
            }

            buffer[7] >>=1; // shift decode buffer right by 1

            // stay in bounds
            if (counter < this8BitArray.size())
            {
                // byte is decoded, stuff it into the array
                decodedArray.append(buffer[thisByte]);
                //DM_OUT << "8bit - packet: " << thisPacket << "Byte: " << thisByte << " data: " << buffer[thisByte];
            }
            counter++;
        }
    }
    //DM_OUT << "Returning...";
    return decodedArray;
}

// ----------------------------------------------------------
// slots
// ----------------------------------------------------------

bool MidiDeviceManager::slotOpenMidiIn()
{
    DM_OUT << "slotOpenMidiIn called - port: " << port_in;

    if (port_in == -1)
    {
        DM_OUT << "slotOpenMidiIn: ERROR, port does not exist (-1)";
        return 0;
    }

    // first close the port to avoid errors
    if (!slotCloseMidiIn(SIGNAL_NONE)) DM_OUT << "couldn't close in port: " << port_in;

    //midi_in = new RtMidiIn(); // refresh RtMidi

    try
    {

        // setup RtMidi connections

        //open ports
        midi_in->openPort(port_in);

        // setup callback
        DM_OUT << "setting callback";
        midi_in->setCallback( &MidiDeviceManager::midiInCallback, this);
        callbackIsSet = true;

        // Don't ignore sysex, timing, or active sensing messages.
        midi_in->ignoreTypes( false, false, false );
    }
    catch (RtMidiError &error)
    {
        /* Return the error */
        DM_OUT << "OPEN MIDI IN ERR:" << (QString::fromStdString(error.getMessage()));
        port_in_open = false;
        portName_in = "";
        connected = false;
        bootloaderMode = false;
        //delete midi_in;
        //midi_in = nullptr;
        return 0;
    }

    portName_in = kmiPorts->getInPortName(port_in);
    port_in_open = true;

    if (PID == PID_AUX && !connected) // aux ports don't need firmware to match for connect
    {
        DM_OUT << "Connected MIDI IN";
        connected = true;
        emit signalConnected(true);
    }
#ifdef MDM_DEBUG_ENABLED
    else
    {
        DM_OUT << "Could not connect MIDI IN - PID: " << PID << " connected: " << connected;
    }
#endif
    return 1;
}

bool MidiDeviceManager::slotOpenMidiOut()
{
    DM_OUT << "slotOpenMidiOut called - port: " << port_out;

    if (port_out == -1)
    {
        DM_OUT << "slotOpenMidiOut: ERROR, port does not exist (-1)";
        return 0;
    }

    // first close the port to avoid errors
    if (!slotCloseMidiOut(SIGNAL_NONE)) DM_OUT << "couldn't close out port: " << port_out;

    //midi_out = new RtMidiOut(); // refresh instance

    try
    {
        //open ports
        midi_out->openPort(port_out);
    }
    catch (RtMidiError &error)
    {
        /* Return the error */
        DM_OUT << "OPEN MIDI OUT ERR:" << (QString::fromStdString(error.getMessage()));
        port_out_open = false;
        portName_in = "";
        connected = false;
        bootloaderMode = false;
        //delete midi_out;
        //midi_out = nullptr;
        return 0;
    }
    portName_out = kmiPorts->getOutPortName(port_out);
    port_out_open = true;

    if (PID == PID_AUX && !connected) // aux ports don't need firmware to match for connect
    {
        DM_OUT << "Connected MIDI Out";
        connected = true;
        emit signalConnected(true);
    }
#ifdef MDM_DEBUG_ENABLED
    else
    {
        DM_OUT << "Could not connect MIDI OUT - PID: " << PID << " connected: " << connected;
    }
#endif
    return 1;
}

bool MidiDeviceManager::slotCloseMidiIn() // no argument, default doesn't send a disconnect signal
{
    return slotCloseMidiIn(SIGNAL_NONE);
}

bool MidiDeviceManager::slotCloseMidiIn(bool signal) // SIGNAL_SEND is the most common usage
{
    DM_OUT << "slotCloseMidiIn called, send disconnect signal: " << signal;

    // alert host application that we are disconnected
    bootloaderMode = false;
    port_in_open = false;
    portName_in = "";

    if (connected) // check so we only emit once
    {
        connected = false;
        if (signal == SIGNAL_SEND) emit signalConnected(false);
    }

    if (midi_in != nullptr)
    {
        //DM_OUT << "midi_in exists: " << midi_in;
    }
    else
    {
        DM_OUT << "WARNING: midi_in is not instantiated, assuming port is closed";
        return 0; // handler doesn't exist
    }

    try
    {
        //close ports
        midi_in->closePort();
        if (callbackIsSet)
        {
            DM_OUT << "cancelling callback";
            midi_in->cancelCallback();
            callbackIsSet = false;
        }
//        DM_OUT << "deleting midi_in";
//        delete midi_in;
//        midi_in = new RtMidiIn();

        //midi_in = nullptr;
    }
    catch (RtMidiError &error)
    {
        /* Return the error */
        DM_OUT << "CLOSE MIDI IN ERR:" << (QString::fromStdString(error.getMessage()));
        return 0;
    }
    return 1;
}

bool MidiDeviceManager::slotCloseMidiOut() // no argument, default doesn't send a disconnect signal
{
    return slotCloseMidiOut(SIGNAL_NONE);
}


bool MidiDeviceManager::slotCloseMidiOut(bool signal)
{
    DM_OUT << "slotCloseMidiOut called, send disconnect signal: " << signal;

    bootloaderMode = false;
    port_out_open = false;
    portName_out = "";

    // alert host application that we are disconnected
    if (connected) // check so we only emit once
    {
        connected = false;
        if (signal == SIGNAL_SEND) emit signalConnected(false);
    }

    if (midi_out == nullptr)
    {
        DM_OUT << "WARNING: midi_out was not instantiated, assuming port is closed";
        return 1; // handler doesn't exist
    }

    try
    {
        //close ports
        midi_out->closePort();
//        DM_OUT << "deleting midi_out";
//        delete midi_out;
//        midi_out = new RtMidiOut();
        //midi_out = nullptr;
    }
    catch (RtMidiError &error)
    {
        /* Return the error */
        DM_OUT << "CLOSE MIDI OUT ERR:" << (QString::fromStdString(error.getMessage()));
        return 0;
    }
    return 1;
}

// -----------------------------------------------------------
// Virtual ports
// -----------------------------------------------------------

#ifndef Q_OS_WIN
bool MidiDeviceManager::slotCreateVirtualIn(QString portName)
{
    DM_OUT << "slotCreateVirtualIn called";

    try
    {
        // first close the port to avoid errors
        if (!slotCloseMidiIn(SIGNAL_NONE)) DM_OUT << "couldn't close in port: " << port_in;

        //midi_in = new RtMidiIn(); // refresh RtMidi

        // create/open port
        midi_in->openVirtualPort(portName.toStdString());

        // setup callback
        midi_in->setCallback( &MidiDeviceManager::midiInCallback, this);
        callbackIsSet = true;

        // Don't ignore sysex, timing, or active sensing messages.
        midi_in->ignoreTypes( false, false, false );
    }
    catch (RtMidiError &error)
    {
        DM_OUT << "openVirtualInPort error:" << (QString::fromStdString(error.getMessage()));
        port_in_open = false;
        portName_in = "";
        connected = false;
        bootloaderMode = false;
        //delete midi_in;
        //midi_in = nullptr;
        return 0;
    }

    portName_in = portName;
    port_in_open = true;
    connected = true;
    emit signalConnected(true);
    return 1;
}

bool MidiDeviceManager::slotCreateVirtualOut(QString portName)
{
    DM_OUT << "slotCreateVirtualOut called";

    try
    {
        // first close the port to avoid errors
        if (!slotCloseMidiOut(SIGNAL_NONE)) DM_OUT << "couldn't close out port: " << port_out;

        //midi_out = new RtMidiOut(); // refresh instance

        // create/open ports
        midi_out->openVirtualPort(portName.toStdString());
    }
    catch (RtMidiError &error)
    {
        DM_OUT << "openVirtualOutPort error:" << (QString::fromStdString(error.getMessage()));
        port_out_open = false;
        portName_in = "";
        connected = false;
        bootloaderMode = false;
        //delete midi_out;
        //midi_out = nullptr;
        return 0;
    }
    portName_out = portName;
    port_out_open = true;
    connected = true;
    emit signalConnected(true);
    return 1;
}
#endif

// -----------------------------------------------------------
// reset connections
// -----------------------------------------------------------

// reset connections is needed when the bootloader and app port names don't match
//void MidiDeviceManager::slotResetConnections(QString portNameApp, QString portNameBootloader)
//{
//    DM_OUT << "slotResetConnections called - portName: " << portNameApp << " portNameBootloader: " << portNameBootloader << "bootloaderMode: " << bootloaderMode;
//    bool refreshDone = false;
//    bool initialBootloaderMode = bootloaderMode;
//    int cycleCount = 0;

//    QString thisPortName;

//    QString lastPortName = kmiPorts->getInPortName(port_in);

//    unsigned char numInPorts, numOutPorts;

////    if (midi_in != nullptr)
////    {
////        midi_in->cancelCallback();
////    }
////    else
////    {
////        DM_OUT << "WARNING: midi_in does not exist, aborting cancel callback";
////    }

//    pollingStatus = false;

////    DM_OUT << "Closing ports..." << lastPortName;

////    slotCloseMidiIn();
////    slotCloseMidiOut();

//    //if (midi_in != nullptr)
//        //midi_in->closePort();
//    //if (midi_out != nullptr)
//        //midi_out->closePort();

//    //DM_OUT << "deleting ports...";
//    //delete midi_out;
//    //delete midi_in;

//    // safety for other threads to chck
//    //midi_in = nullptr;
//    //midi_out = nullptr;

//    refreshTimer.restart();

//    failFlag = false;
//    while (!refreshDone && !failFlag) // loop until the port changes
//    {
//        // this is a hack for now, KMI_Ports should be reporting these changes, investigate this further
//        DM_OUT << "\n";
//        DM_OUT << "#######################################################";
//        DM_OUT << "slotResetConnections - getPortCount - start refreshTimer";
//        DM_OUT << "#######################################################";

////        if (midi_in == nullptr)
////        {
////            //DM_OUT << "new midi_in instance...";
////            midi_in = new RtMidiIn(); // refresh instance
////        }
////        else
////        {
////            midi_in->cancelCallback();
////        }
////        if (midi_out == nullptr)
////        {
////            //DM_OUT << "new midi_out instance...";
////            midi_out = new RtMidiOut(); // refresh instance
////        }

////        try
////        {
////            midi_in->setCallback( &MidiDeviceManager::midiInCallback, this);
////        }
////        catch (RtMidiError &error)
////        {
////            /* Return the error */
////            DM_OUT << "Error cancelling/setting callback - Message: " << QString::fromStdString(error.getMessage());
////        }

////        midi_in->ignoreTypes( false, false, false );

//        numInPorts = kmiPorts->IN_PORT_MGR->getPortCount();
//        numOutPorts = kmiPorts->OUT_PORT_MGR->getPortCount();

//        // we need both an in and an out port to continue
//        if (numInPorts + numOutPorts > 1)
//        {
//            // test/fix input port name
//            for (uint thisPort = 0; thisPort < (numInPorts); thisPort++)
//            {
//                if (thisPort <= numInPorts)
//                {

//                    QString newPortName = kmiPorts->getInPortName(thisPort);

//                    // confirm we are either going bootLoader->app or app->bootLoader
//                    if (initialBootloaderMode && newPortName == portNameApp)
//                    {
//                        DM_OUT << "Bootloader->App - found application input port: " << newPortName << " at port #: " << thisPort;
//                        slotUpdatePortIn(thisPort);
//                    }
//                    else if (!initialBootloaderMode && newPortName == portNameBootloader)
//                    {
//                        DM_OUT << "App->Bootloader - found bootloader input port: " << newPortName << " at port #: " << thisPort;
//                        slotUpdatePortIn(thisPort);
//                    }
//                    else
//                    {
//                        DM_OUT << "Found input port - thisPort: " << thisPort << " newPortName: " << newPortName;
//                    }

//                }
//                else
//                {
//                    DM_OUT << "Port subtraction before end of for loop, numInPorts: " << numInPorts << " thisPort: " << thisPort;
//                }
//            }

//            // test/fix output port name
//            for (uint thisPort = 0; thisPort < (numOutPorts); thisPort++)
//            {
//                if (thisPort <= numOutPorts)
//                {
//                    QString newPortName = kmiPorts->getOutPortName(thisPort);

//                    // confirm we are either going bootLoader->app or app->bootLoader
//                    if (initialBootloaderMode && newPortName == portNameApp)
//                    {
//                        DM_OUT << "Bootloader->App - found application output port: " << newPortName << " at port #: " << thisPort;
//                        slotUpdatePortOut(thisPort);
//                    }
//                    else if (!initialBootloaderMode && newPortName == portNameBootloader)
//                    {
//                        DM_OUT << "App->Bootloader - found bootloader output port: " << newPortName << " at port #: " << thisPort;
//                        slotUpdatePortOut(thisPort);
//                    }
//                    else
//                    {
//                        DM_OUT << "Found output port - thisPort: " << thisPort << " newPortName: " << newPortName;
//                    }
//                }
//                else
//                {
//                    DM_OUT << "Port subtraction before end of for loop, numOutPorts: " << numOutPorts << " thisPort: " << thisPort;
//                }
//            }

//            // attempting to open the ports when we are in between app and bootloader modes helps to flush out the old port settings
////            try
////            {
////                thisPortName = kmiPorts->getInPortName(port_in);

////                DM_OUT << "Opening ports..." << thisPortName << " port_in: " << port_in << " port_out" << port_out;

//////                if (midi_in == nullptr)
//////                {
//////                    //DM_OUT << "new midi_in instance...";
//////                    midi_in = new RtMidiIn(); // refresh instance
//////                }
//////                if (midi_out == nullptr)
//////                {
//////                    //DM_OUT << "new midi_in instance...";
//////                    midi_out = new RtMidiOut(); // refresh instance
//////                }
////                midi_in->openPort(port_in);
////                midi_out->openPort(port_out);
////            }
////            catch (RtMidiError &error)
////            {
////                /* Return the error */
////                thisPortName = ""; // prevents refreshDone being set to true in next if statement
////                DM_OUT << "Port #" << port_in << " not found, retrying. Error Message: " << QString::fromStdString(error.getMessage());

////            }

//            // *****************************************
//            // validate if the ports have been refreshed
//            // *****************************************
//            if (!initialBootloaderMode && portName_in == portNameBootloader && portName_out == portNameBootloader) // test for app->bootloader
//            {
//                DM_OUT << "APP -> BOOTLOADER CYCLE COMPLETE - input port: " << portName_in;

//                if (portName_in == SS_BL_PORT) slotUpdatePID(PID_SOFTSTEP_BL); // hack for softstep
//                bootloaderMode = true;
//                refreshDone = true;
//                pollingStatus = true; // app->bootloader, begin polling
//            }
//            else if (initialBootloaderMode && portName_in == portNameApp && portName_out == portNameApp) // test if bootloader->app
//            {
//                DM_OUT << "BOOTLOADER -> APP CYCLE COMPLETE - input port: " << portName_in;

//                if (portName_in == SS_IN_P1) slotUpdatePID(PID_SOFTSTEP2); // hack for softstep
//                bootloaderMode = true;
//                refreshDone = true;
//                pollingStatus = true; // bootloader->app, begin polling
//            }
//            else // we are about to loop back for up to 45 seconds - close the ports, which is necessary to flush out the old settings
//            {

//                DM_OUT << "No port match - initialBootloaderMode: " << initialBootloaderMode << " portName_in: " << portName_in << " portName_out: " << portName_out;


//            } // end validate if ports have been refreshed
//        } // end if (numInPorts + numOutPorts > 1)
//        refreshTimer.restart();
//        int lastTick = 0;

//        while(refreshTimer.elapsed() < 1000)
//        {
//            if (refreshTimer.elapsed() > (lastTick + 100))
//            {
//                lastTick = refreshTimer.elapsed();
//                //DM_OUT << "Elapsed: " << lastTick;
//            }
//        }
//        if (cycleCount > 44)
//        {
//            failFlag = true; // break the llop
//        }
//        else
//        {
////                    DM_OUT << "Closing ports..." << thisPortName;
////                    if (midi_in != nullptr) midi_in->closePort();
////                    if (midi_out != nullptr) midi_out->closePort();

//            //DM_OUT << "deleting ports... ";
//            DM_OUT << "";
//            DM_OUT << "------------------------------------------------------";
//            DM_OUT << "cycleCount: " << ++cycleCount << " time elapsed: " << refreshTimer.elapsed();
//            DM_OUT << "------------------------------------------------------\n";
//            //delete midi_out;
//            //delete midi_in;

//            //midi_in = nullptr; // re-instantiate these on the loop back in
//            //midi_out = nullptr;
//        }
//    } // end while (!refreshDone && !failFlag)

//    // safety check
////    if (midi_in == nullptr)
////    {
////        //DM_OUT << "new midi_in instance...";
////        midi_in = new RtMidiIn(); // refresh instance
////    }
////    else
////    {
////        midi_in->cancelCallback();
////    }
////    if (midi_out == nullptr)
////    {
////        //DM_OUT << "new midi_out instance...";
////        midi_out = new RtMidiOut(); // refresh instance
////    }

//// brute force, but necessary to reset the ports
//#ifdef Q_OS_WIN
//    // restart:

//    if (!failFlag)
//    {
//        restart = true;

//        QMessageBox msgBox;
//        if (initialBootloaderMode) // if we are going bootloader->app
//        {
//            emit signalFwProgress(100);

//            msgBox.setText(QString("Firmware update sent.\n\nThe application must now re-start to refresh the MIDI driver and connect to the %1.").arg(deviceName));
//        }
//        else // app->bootloader
//        {
//            msgBox.setText("Bootloader command sent.\n\nThe application must now re-start to refresh the MIDI driver.");
//        }
//        msgBox.setStandardButtons(QMessageBox::Ok);
//        msgBox.setDefaultButton(QMessageBox::Ok);
//        msgBox.exec();

//        qApp->quit();
//        QProcess::startDetached(qApp->arguments()[0], qApp->arguments());
//    }
//#endif

//    if (failFlag == true)
//    {
//        slotFirmwareUpdateReset();
//        connected = false;
//        emit signalFirmwareUpdateComplete(false);
//        pollingStatus = false;
//        failFlag = false;
//    }
////    else if (initialBootloaderMode)
////    {
////        //slotStartPolling("slotResetConnections - bootloader->app successful");
////        pollingStatus = true; // bootloader->app, begin polling
////    }

//    DM_OUT << "slotResetConnections set callback";

//    try
//    {
//        midi_in->setCallback( &MidiDeviceManager::midiInCallback, this);
//    }
//    catch (RtMidiError &error)
//    {
//        /* Return the error */
//        DM_OUT << "Error setting callback - Message: " << QString::fromStdString(error.getMessage());
//    }
//    midi_in->ignoreTypes( false, false, false );
//}

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
void MidiDeviceManager::slotStartPolling(QString caller)
{
    DM_OUT << "slotStartPolling called - caller:" << caller << " pollingStatus:" << pollingStatus;

    //slotStopPolling("slotStartPolling clearing before enable");

    int pollTime = 1000;

    //versionPoller = new QTimer(this);
    connect(versionPoller, SIGNAL(timeout()), this, SLOT(slotPollVersion()));

//    // some devices take longer to boot up
//    if (deviceName == "12 Step")
//    {
//        pollTime = 4000;
//    }
//    else if (deviceName == "SSCOM" || deviceName == "SoftStep" || deviceName == "SoftStep Bootloader")
//    {
//        pollTime = 4000;
//    }
//    else
//    {
//        pollTime = 2000;
//    }
//    //versionPoller->start(pollTime);
    versionPoller->setInterval(pollTime);

    //pollingStatus = true; // allow polling to proceed
}

void MidiDeviceManager::slotStopPolling(QString caller) // this has to be called from an emitted signal so that it's handled in the main thread
{
    DM_OUT << "slotStopPolling called - caller:" << caller;
    //versionPoller->stop();

    disconnect(this, SLOT(slotPollVersion())); // disconnect all slots/signals for the timer
    //pollingStatus = false; // block polling
}

// The firmware process should be....
// 1. Editor detects/identifies MIDI port
// 2. Editor sets pollingStatus to true, fwVer/SysExID request is sent
// 3. Reply received

// Call this every X seconds
// 1. Test if midi system and ports are set up
// 1. test if we need to send a fw version/identity request
// 2. run through the firmware update state machine switch case

#define FW_UPDATE_TIMEOUT_INTERVAL 35000

void MidiDeviceManager::slotPollVersion()
{
    bool portsAreSetUp = (port_in == -1 || port_out == -1 || !port_in_open || !port_out_open) ? false : true;

    //DM_OUT << "slotPollVersion called - pollingStatus: " << pollingStatus << " firmwareUpdateState: " << firmwareUpdateState << " portsAreSetUp: " << portsAreSetUp << " bootloaderMode:" << bootloaderMode << " fwVerPollSkipConnectCycles: " << fwVerPollSkipConnectCycles;
    //if (!portsAreSetUp) DM_OUT << "port_in: " << port_in << " port_out: " << port_out << " port_in_open: " << port_in_open << " port_out_open: " << port_out_open;

    int remainingSeconds = round((FW_UPDATE_TIMEOUT_INTERVAL - firmwareUpdateStateTimer.elapsed()) / 1000);

    if (firmwareUpdateState > FWUD_STATE_BEGIN && firmwareUpdateStateTimer.elapsed() > (FW_UPDATE_TIMEOUT_INTERVAL - 10000))
    {
        if (remainingSeconds > 0) emit signalFwConsoleMessage(QString("\nTimeout in %1...").arg(remainingSeconds));
    }

    switch (firmwareUpdateState)
    {
    case FWUD_STATE_IDLE:
        break;
    case FWUD_STATE_BEGIN:
        DM_OUT << "Begin Firmware Update Process - fwSaveRestoreGlobals: " << fwSaveRestoreGlobals;
        if (bootloaderMode)
        {
            firmwareUpdateState = FWUD_STATE_BL_MODE;
        }
        else if (fwSaveRestoreGlobals)
        {
            firmwareUpdateState = FWUD_STATE_GLOBALS_REQ_SEND;
        }
        else
        {
            firmwareUpdateState = FWUD_STATE_BL_SEND;
        }
        //DM_OUT << "fwVerPollSkipConnectCycles = 0";
        fwVerPollSkipConnectCycles = 0;
        firmwareUpdateStateTimer.restart();
        break;
    case FWUD_STATE_GLOBALS_REQ_SEND:
        DM_OUT << "Begin globals backup - send request and start timer";
//        globalsRequested = true; // set flag, it gets cleared when firmware completes or times out
        emit signalRequestGlobals();
        emit signalFwProgress(10); // increment progress bar
        emit signalFwConsoleMessage(QString("\n\nBacking up %1 global settings...").arg(deviceName));
        firmwareUpdateState = FWUD_STATE_GLOBALS_REQ_SENT_WAIT;
        firmwareUpdateStateTimer.restart();
        break;
    case FWUD_STATE_GLOBALS_REQ_SENT_WAIT:
        DM_OUT << "Globals request sent, waiting for a response..." << firmwareUpdateStateTimer.elapsed();
        if (remainingSeconds == 25)
        {
            DM_OUT << "Sending Globals request (again)";
            emit signalRequestGlobals();
        }
        else if (remainingSeconds < 15)
        {
            DM_OUT << "No response to globals request, skipping";
            emit signalFwConsoleMessage("\n\nNo response to globals backup request, resetting to default settings and proceeding with firmware update.\n");
            firmwareUpdateState = FWUD_STATE_BL_SEND;
        }
        break;
    case FWUD_STATE_GLOBALS_RCVD:
        DM_OUT << "Globals received";
        pollingStatus = false;
        emit signalFwConsoleMessage("\n\nGlobals Saved.\n");
        emit signalFwProgress(20); // increment progress bar
        firmwareUpdateState = FWUD_STATE_BL_SEND;
        firmwareUpdateStateTimer.restart();
        break;
    case FWUD_STATE_BL_SEND:
        DM_OUT << "Sending bootloader image/command...";

        switch (PID)
        {
        case PID_SOFTSTEP1:
        case PID_SOFTSTEP2:
            if ((uchar)deviceFirmwareVersion[0] < 1) // pre-bootloader firmware
            {
                // version 98 is a placeholder for ZenDesk users and has a bootloader
                // version 99 is the trojan horse bootloader update
                // everything else needs to have the bootloader installed
                if (((uchar)deviceFirmwareVersion[0] == 9 && (uchar)deviceFirmwareVersion[1] < 8) ||
                     (uchar)deviceFirmwareVersion[0] < 9)
                {
                    emit signalFwConsoleMessage("\n\n*** Installing bootloader *** - device will reboot several times!\n");

                    DM_OUT << "fwVerPollSkipConnectCycles = 2";
                    fwVerPollSkipConnectCycles = 2; // don't send fw version request during bootloader install

                    // this will install the firmware and reboot the device into bootloader mode
                    slotSendSysExBA(bootloaderByteArray);
                }
            }
            else // this should be the standard method moving forward
            {
                emit signalFwConsoleMessage("\nSending Enter bootloader Command, device will reboot.\n");
                slotSendSysEx(_bl_softstep, sizeof(_bl_softstep));
            }

            break;
        case PID_QUNEXUS:
            slotSendSysEx(_bl_qunexus, sizeof(_bl_qunexus));
            break;
        case PID_QUNEO:
            slotSendSysEx(_bl_quneo, sizeof(_bl_quneo));
            break;
        default:
            DM_OUT << "Bootloader command not configured for this device";
        }

        if (PID != PID_SOFTSTEP1 && PID != PID_SOFTSTEP2)
        {
            emit signalFwConsoleMessage("\nSending Enter bootloader Command, device will reboot.\n");
        }

        emit signalFwProgress(30); // increment progress bar

        firmwareUpdateState = FWUD_STATE_BL_SENT_WAIT;
        firmwareUpdateStateTimer.restart();
        break;
    case FWUD_STATE_BL_SENT_WAIT:
        DM_OUT << "Bootloader image/command sent, waiting..." << firmwareUpdateStateTimer.elapsed();

        if (remainingSeconds == 20 ||remainingSeconds == 10)
        {
            kmiPorts->slotRefreshPortMaps(); // kick it
        }
        if (remainingSeconds == 25 || remainingSeconds == 15 || remainingSeconds == 5)
        {
            DM_OUT << "Sending SysEx ID version request (again)";
            slotSendSysEx(_sx_id_req_standard, sizeof(_sx_id_req_standard));
        }

        if (firmwareUpdateStateTimer.elapsed() > FW_UPDATE_TIMEOUT_INTERVAL)
        {
            firmwareUpdateState = FWUD_STATE_FAIL;
        }
        break;
    case FWUD_STATE_BL_MODE:
        DM_OUT << "Device in bootloader mode";

        emit signalFwConsoleMessage("\nDevice bootloader detected.\n"); // confirm enter bootloader and next line
        emit signalFwProgress(40); // increment progress bar
        firmwareUpdateState = FWUD_STATE_FW_SEND;
        //DM_OUT << "fwVerPollSkipConnectCycles = 0";
        fwVerPollSkipConnectCycles = 0;
        break;
    case FWUD_STATE_FW_SEND:
        DM_OUT << "Sending Firmware...";

        if (port_out_open == false)
        {
#ifndef Q_OS_WIN
            emit signalFwConsoleMessage(QString("\n%1 not connected!\n").arg(deviceName));
#else
            emit signalFwConsoleMessage(QString("\n%1 MIDI driver not connected or unavailable.\n"
                                   "Windows cannot share standard USB MIDI drivers, try\n"
                                   "closing all programs, re-starting the %1 editor,\n"
                                   "and then reconnecting your %1.\n").arg(deviceName));
#endif
            firmwareUpdateState = FWUD_STATE_FAIL;
        }
        emit signalFwConsoleMessage("\nUpdating Firmware...\n");
        emit signalFwProgress(50); // increment progress bar

        if (firmwareByteArray.length() < 1)
        {
            DM_OUT << "Firmware file not defined!";
            emit signalFwConsoleMessage("\nERROR! Firmware file not found!\n");
            firmwareUpdateState = FWUD_STATE_FAIL;
            firmwareUpdateStateTimer.restart();
            return; // no file, should trip an error
        }

        DM_OUT << "sending firmware sysex";

        slotSendSysExBA(firmwareByteArray); // send firmware
        firmwareUpdateState = FWUD_STATE_FW_SENT_WAIT;
        firmwareUpdateStateTimer.restart();
        break;
    case FWUD_STATE_FW_SENT_WAIT:
        DM_OUT << "Firmware Image sent, waiting for device to reboot..." << firmwareUpdateStateTimer.elapsed();

        if (remainingSeconds == 20 ||remainingSeconds == 10)
        {
            kmiPorts->slotRefreshPortMaps(); // kick it
        }
        if (remainingSeconds == 25 || remainingSeconds == 15 || remainingSeconds == 5)
        {
            DM_OUT << "Sending SysEx ID version request (again)";
            slotSendSysEx(_sx_id_req_standard, sizeof(_sx_id_req_standard));
        }

        if (firmwareUpdateStateTimer.elapsed() > FW_UPDATE_TIMEOUT_INTERVAL)
        {
            firmwareUpdateState = FWUD_STATE_FAIL;
        }
        break;
    case FWUD_STATE_GLOBALS_SEND:
        emit signalRestoreGlobals(); // editor will handle this message
        emit signalFwProgress(90); // increment progress bar
        emit signalFwConsoleMessage("\nRestoring Globals...\n");
        firmwareUpdateState = FWUD_STATE_SUCCESS;
        firmwareUpdateStateTimer.restart();
        break;
    case FWUD_STATE_SUCCESS:
        DM_OUT << "Firmware Update Successful!" << firmwareUpdateStateTimer.elapsed();

        emit signalFirmwareUpdateComplete(true);
        firmwareUpdateState = FWUD_STATE_IDLE;
        //DM_OUT << "fwVerPollSkipConnectCycles = 0";
        fwVerPollSkipConnectCycles = 0;
        connected = true;
        break;
    case FWUD_STATE_FAIL:
        //DM_OUT << "Firmware Update Failed!!" << firmwareUpdateStateTimer.elapsed();
        slotFirmwareUpdateReset();
        connected = false;
        emit signalFirmwareUpdateComplete(false);
        firmwareUpdateState = FWUD_STATE_IDLE;
        fwVerPollSkipConnectCycles = 0;
        //DM_OUT << "fwVerPollSkipConnectCycles = 0";
        break;
    }

    // Send fwVer/identity request?
    if (pollingStatus && portsAreSetUp)
    {
        // test if our ports still match
        int newPortIn = kmiPorts->getInPortNumber(portName_in);
        int newPortOut = kmiPorts->getOutPortNumber(portName_out);

        if (port_in != kmiPorts->getInPortNumber(portName_in))
        {
            DM_OUT << "ERROR: input port: " << port_in << " does not match RtMidi port: " << newPortIn << " - updating...";
            if (!slotUpdatePortIn(newPortIn))
            {
                DM_OUT << "ERROR: couldn't update input port";
            }
        }

        if (port_out != kmiPorts->getOutPortNumber(portName_out))
        {
            DM_OUT << "ERROR: output port: " << port_out << " does not match RtMidi port: " << newPortOut << " - updating...";
            if (!slotUpdatePortOut(newPortOut))
            {
                DM_OUT << "ERROR: couldn't update output port";
            }
        }

        if (firstFwVerRequestHasBeenSent == false || fwVerRequestTimer.elapsed() > 5000) // only send a request once every 5 seconds
        {
            bool requestSent = false;

            if (fwVerPollSkipConnectCycles > 0) // don't send request while bootloader installs
            {
                DM_OUT << "Blocking firmware version request - fwVerPollSkipConnectCycles: " << fwVerPollSkipConnectCycles;
            }
            else if (deviceName == "SSCOM") // old softStep firmware doesn't use the universal syx dev id request
            {
                DM_OUT << "Sending SSCOM firmware version request";
                slotSendSysEx(_fw_req_softstep, sizeof(_fw_req_softstep));
                requestSent = true;
            }
            else if (deviceName == "12 Step") // 12 step also doesn't use the universal syx dev id request
            {
                DM_OUT << "Sending 12 Step firmware version request";
                slotSendSysEx(_fw_req_12step, sizeof(_fw_req_12step));
                requestSent = true;
            }
            else // every other product
            {
                DM_OUT << "Sending SysEx ID version request";
                slotSendSysEx(_sx_id_req_standard, sizeof(_sx_id_req_standard));
                requestSent = true;
            }
            firstFwVerRequestHasBeenSent = true;
            fwVerRequestTimer.restart();
            if (requestSent == true)
            {
                emit signalFwConsoleMessage("\nRequesting firmware version from device...");
            }
        }
    }
}

//void MidiDeviceManager::slotStartGlobalsTimer()
//{
//    DM_OUT << "slotStartGlobalsTimer called";
//    globalsTimerCount = 0;
//    timeoutGlobalsReq = new QTimer(this);
//    connect(timeoutGlobalsReq, SIGNAL(timeout()), this, SLOT(slotCheckGlobalsReceived()));
//    timeoutGlobalsReq->start(1000); // ping every second to see if globals have been received
//}

//void MidiDeviceManager::slotStopGlobalTimer()
//{
//    timeoutGlobalsReq->stop(); // stop timer
//}

void MidiDeviceManager::slotCheckGlobalsReceived()
{
//    DM_OUT << "slotCheckGlobalsReceived called - globalsTimerCount: " << globalsTimerCount;

//    if (globalsTimerCount < 5) // under timeout threshold
//    {
//        emit signalRequestGlobals(); // ping again
//        globalsTimerCount++;
//    }
//    else
//    {
//        DM_OUT << "globals request timeout";
//        emit signalStopGlobalTimer();
//        //slotFirmwareUpdateReset(); // reset flags
//        emit signalFwConsoleMessage("\nERROR: No response to Globals request.");
//        emit signalFirmwareUpdateComplete(false); // signal failure
//        failFlag = true;
//    }
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
    DM_OUT << "Send sysex, length: " << len << " syx: " << sysEx << " PID: " << PID;
    std::vector<unsigned char> message(sysEx, sysEx+len);

    ioGate = false; // pause any midi output while sending SysEx

    if (port_out_open == false)
    {
        DM_OUT << "ERROR: midi_out is not open, aborting slotSendMIDI!";
        return; // handler doesn't exist
    }

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
// slotProcessSysEx - parse incomming sysex
// - detect firmware/id responses and update firmwareUpdateState
// - pass along all other sysex messages
// *************************************************
void MidiDeviceManager::slotProcessSysEx(QByteArray sysExMessageByteArray, std::vector< unsigned char > *sysExMessageCharArray)
{
    DM_OUT << "slotProcessSysEx called - PID: " << PID << " deviceName: " << deviceName << " length: " << sysExMessageByteArray.length();

    // ********************************************
    // Test for feedback loop
    // ********************************************

    QByteArray feedbackLoopTest(reinterpret_cast<char*>(_sx_ack_loop_test), sizeof(_sx_ack_loop_test));
    int feedbackTestIndex = sysExMessageByteArray.indexOf(feedbackLoopTest, 0);

    if (feedbackTestIndex == 0) // feedback loop detected!
    {
        DM_OUT << "*** FEEDBACK LOOP DETECTED, MIDI PORTS CLOSED *** - " << sysExMessageByteArray;
        this->disconnect(SIGNAL(signalRxMidi_raw(uchar, uchar, uchar, uchar)));
        slotCloseMidiIn(SIGNAL_SEND); // better than letting the app crash? Only if we alert the end user, otherwise this becomes a support
        slotCloseMidiOut(SIGNAL_SEND); // better than letting the app crash? Only if we alert the end user, otherwise this becomes a support
        emit signalFeedbackLoopDetected(this);
        slotErrorPopup("MIDI FEEDBACK LOOP DETECTED\nPorts Closed");
    }

    // ********************************************
    // Define firmware version metadata
    // ********************************************

    // Query for "pre-bootloader" SoftStep replies
    QByteArray fwQueryReplySS(reinterpret_cast<char*>(_fw_reply_softstep), sizeof(_fw_reply_softstep));
    int replyIndexSS = sysExMessageByteArray.indexOf(fwQueryReplySS, 0);

    // Query for 12 Step
    QByteArray fwQueryReply12S(reinterpret_cast<char*>(_fw_reply_12step), sizeof(_fw_reply_12step));
    int replyIndex12S = sysExMessageByteArray.indexOf(fwQueryReply12S, 0);

    // Query for all other devices (test 12Step)
    QByteArray fwQueryReply(reinterpret_cast<char*>(_sx_id_reply_standard), sizeof(_sx_id_reply_standard));
    int replyIndex = sysExMessageByteArray.indexOf(fwQueryReply, 0);

    // ********************************************
    // PROCESS DEVICE SPECIFIC VERSION MESSAGES
    // ********************************************

    // ***** Soft Step Pre-Bootloader **************************************
    if (replyIndexSS == 2)
    {
        pollingStatus = false; // turn off version polling

        // this is the old softstep reply
        deviceName = "SSCOM";
        DM_OUT << "SoftStep old fw (no bootloader) reply:" <<  sysExMessageByteArray;


        int fwVerWhole = (uchar)sysExMessageByteArray.at(68);

        // no bootloader
        deviceFirmwareVersion[2] = fwVerWhole % 10; // last digit
        deviceFirmwareVersion[1] = (fwVerWhole - (uchar)deviceFirmwareVersion[2]) / 10; // second digit
        deviceFirmwareVersion[0] = 0;

        devicebootloaderVersion[2] = 0;
        devicebootloaderVersion[1] = 0;
        devicebootloaderVersion[0] = 0;

        DM_OUT << QString("SoftStep fw ver: %1.%2.%3").arg((uchar)deviceFirmwareVersion[0]).arg((uchar)deviceFirmwareVersion[1]).arg((uchar)deviceFirmwareVersion[2]);
        DM_OUT << QString("SoftStep bl ver: %1.%2.%3").arg((uchar)devicebootloaderVersion[0]).arg((uchar)devicebootloaderVersion[1]).arg((uchar)devicebootloaderVersion[2]);
    }

    // ***** 12 Step ****************************************
    else if (replyIndex12S == 1)
    {
        pollingStatus = false; // turn off polling

        // EB TODO: this is a temporary cluge to make the editor work with old, non-bootloader firmware. Remove when fw1.0.0 is out
        bootloaderMode = false;

        int fwVerWhole = (uchar)sysExMessageByteArray.at(68);

        // no bootloader
        deviceFirmwareVersion[2] = fwVerWhole % 10; // last digit
        deviceFirmwareVersion[1] = (fwVerWhole - (uchar)deviceFirmwareVersion[2]) / 10; // second digit
        deviceFirmwareVersion[0] = 0;

        devicebootloaderVersion[2] = 0;
        devicebootloaderVersion[1] = 0;
        devicebootloaderVersion[0] = 0;

        DM_OUT << QString("12Step fw ver: %1.%2.%3").arg((uchar)deviceFirmwareVersion[0]).arg((uchar)deviceFirmwareVersion[1]).arg((uchar)deviceFirmwareVersion[2]);
    }

    // ***** QuNeo ****************************************
    else if (replyIndex == 0 && deviceName == "QuNeo")
    {
        pollingStatus = false; // turn off polling

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
        pollingStatus = false; // turn off polling

        devicebootloaderVersion[0] = sysExMessageByteArray[12];
        devicebootloaderVersion[1] = sysExMessageByteArray[13];
        devicebootloaderVersion[2] = sysExMessageByteArray[14];

        deviceFirmwareVersion[0] = sysExMessageByteArray[15];
        deviceFirmwareVersion[1] = sysExMessageByteArray[16];
        deviceFirmwareVersion[2] = sysExMessageByteArray[17];

        PID_MIDI = (uchar)sysExMessageCharArray->at(8); // store the MIDI PID - added for SoftStep to differentiate version 1 vs 2

        if ((unsigned char)sysExMessageByteArray.at(9) == 1)
        {
            bootloaderMode = true;
            if (!deviceName.contains("Bootloader"))
            {
                deviceName = deviceName.append(" Bootloader");
            }
        }
        else
        {
            bootloaderMode = false;
            slotUpdatePID(PID_MIDI);
        }

        DM_OUT << "ID Reply - PID_MIDI: " << PID_MIDI << " BL: " << devicebootloaderVersion << " FW: " << deviceFirmwareVersion; // << " fullMsg: " << sysExMessageByteArray;
    }
    // ********************************************
    // process non fw/id SysEx Messages
    // ********************************************
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

        //DM_OUT << "passing SysEx to applicaiton";
        // send SysEx to application
        emit signalRxSysExBA(sysExMessageByteArray);
        emit signalRxSysEx(sysExMessageCharArray);

        // leave function
        return;
    }

    // ********************************************
    // Evaluate fw version reply data
    // ********************************************


//    // only allow 1 version response every 2 seconds
//    if (!versionReplyTimer.hasExpired(2000) && firstFwResponseReceived == true) return;

//    firstFwResponseReceived = true; // now we can use the timer
//    versionReplyTimer.restart();

    // ********************************************
    // process firmware version connection messages
    // ********************************************

    //pollingStatus = false;

    if (bootloaderMode)
    {
//        DM_OUT << "Device in bootloader mode";
//        emit signalBootloaderMode(fwUpdateRequested);

        // update firmwareUpdateState if it isn't idle
        if (firmwareUpdateState)
        {
            firmwareUpdateState = FWUD_STATE_BL_MODE;
        }
        else
        {
            emit signalBootloaderMode(false); // trigger the bootloader user prompt/dialog
        }
    }
    // firmware matches
    else if (deviceFirmwareVersion == applicationFirmwareVersion)
    {
        DM_OUT << "emit fw match - fwv: " << deviceFirmwareVersion << "cfwv: " << applicationFirmwareVersion;

        // update firmwareUpdateState if it isn't idle
        if (firmwareUpdateState)
        {
            //
            if (firmwareUpdateState >= FWUD_STATE_FW_SENT_WAIT) // if at any point after sending the firmware packet we get a match, then success
            {
                if (fwSaveRestoreGlobals == true)
                {
                    firmwareUpdateState = FWUD_STATE_GLOBALS_SEND;
                }
                else
                {
                    firmwareUpdateState = FWUD_STATE_SUCCESS;
                }
            }
        }
        else // not updating firmware, connect to editor
        {
            emit signalFirmwareDetected(this, true);
            emit signalConnected(true);
            connected = true;
        }
    }
    else // firmware does not match, alert app/user
    {
        DM_OUT << "emit fw mismatch - fwv: " << deviceFirmwareVersion.toUInt() << "cfwv: " << applicationFirmwareVersion.toUInt();
        emit signalFirmwareDetected(this, false);
    }

}

bool MidiDeviceManager::slotOpenFirmwareFile(QString filePath)
{
    DM_OUT << "slotOpenFirmwareFile called, file: " << filePath;
    //Load Firmware File into a byte array
    firmware = new QFile(filePath);
    if (firmware->open(QIODevice::ReadOnly))
    {
        firmwareByteArray = firmware->readAll();
        return true;
    }
    else
    {
        return false;
    }
}

bool MidiDeviceManager::slotOpenBootloaderFile(QString filePath)
{
    DM_OUT << "slotOpenBootloaderFile called, file: " << filePath;
    //Load Firmware File into a byte array
    bootloader = new QFile(filePath);
    if (bootloader->open(QIODevice::ReadOnly))
    {
        bootloaderByteArray = bootloader->readAll();
        return true;
    }
    else
    {
        return false;
    }
}

// set up the firmware update process - enter bootloader, wait, send ud
void MidiDeviceManager::slotRequestFirmwareUpdate()
{
//    DM_OUT << "slotRequestFirmwareUpdate - fwUpdteRequested: " << fwUpdateRequested << " bootloaderMode: " << bootloaderMode << " globalsRequested: " << globalsRequested;

    firmwareUpdateState = FWUD_STATE_BEGIN;
}

void MidiDeviceManager::slotFirmwareUpdateReset()
{
    DM_OUT << "slotFirmwareUpdateReset called";
//    globalsRequested = false;
    pollingStatus = false;
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
    //DM_OUT << "slotSend MIDI - restart: " << restart << " connected: " << connected;
    if (restart || !connected) return; // added connection check

    uchar newStatus = status + (chan < 16 ? chan : 0); // combine status byte with any valid channel data

//#ifdef MDM_DEBUG_ENABLED
    DM_OUT << QString("slotSendMIDI called - status: %1 d1: %2 d2: %3 channel: %4").arg(status).arg(d1).arg(d2).arg(chan);
//#endif

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
        if ((chan != 255 && chan > 127) || d1 > 127 || d2 > 127) return; // catch bad data
        packet.push_back(newStatus);
        packet.push_back(d1);
        packet.push_back(d2);
        break;
    // two byte packets
    case MIDI_PROG_CHANGE:
    case MIDI_CHANNEL_PRESSURE:
        if ((chan != 255 && chan > 127) || d1 > 127) return; // catch bad data
        packet.push_back(newStatus);
        packet.push_back(d1);
        break;
    default:

        // **********************************
        // ****** SYS COMMON MESSAGES *******
        // **********************************

        //DM_OUT << "sysCommon: " << newStatus;
        switch (newStatus)
        {

        // three byte packets
        case MIDI_MTC:
        case MIDI_SONG_POSITION:
            if (d1 > 127 || d2 > 127) return; // catch bad data
            packet.push_back(newStatus);
            packet.push_back(d1);
            packet.push_back(d2);
            break;
        // two byte packets
        case MIDI_SONG_SELECT:
            if (d1 > 127) return; // catch bad data
            packet.push_back(newStatus);
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
            packet.push_back(newStatus);
            break;
        // catch undefined and bad messages
        default:
            return; // go no further
            break;
        } // end switch (sysCommon)
    } // end switch (status)

#ifdef MDM_DEBUG_ENABLED
    if (status != 254) DM_OUT << "Send MIDI - packet: " << packet;
#endif

    if (port_out_open == false)
    {
        DM_OUT << "ERROR: midi_out is not open, aborting slotSendMIDI! - Status: " << status;
        return; // handler doesn't exist
    }

    // prepare and send the packet
    try
    {
        midi_out->sendMessage( &packet );
    }
    catch (RtMidiError &error)
    {
        QString errorString = QString("MIDI SEND ERR: %1 \nStatus: %2").arg(QString::fromStdString(error.getMessage()), QString::number(status));
        DM_OUT << errorString;
        emit signalFwConsoleMessage(errorString);
    }
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
    if (deltatime == 0 && 1 == 0) return; // suppress warning

    // create pointer for the mdm that called this
    MidiDeviceManager * thisMidiDeviceManager;
    thisMidiDeviceManager = (MidiDeviceManager*) thisCaller;

    if (message->at(0) != 248) // ignore clock
    {
#ifdef MDM_DEBUG_ENABLED
        DM_OUT_P << "midi in - time: " << deltatime << "length: " << message->size();
#endif
    }

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
#ifdef MDM_DEBUG_ENABLED
                if (message->at(0) != 248) // ignore clock
                    DM_OUT_P << "Byte[" << i <<"]: " << message->at(i);
#endif
            }

            //DM_OUT_P << "midi in - message size: " << message->size() << " byte0: " << (uchar)packetArray[0];

            // standard messages
            if ((uchar)packetArray[0] != (uchar)MIDI_SX_START)
            {
                if (message->at(0) != 248) // ignore clock
                {
#ifdef MDM_DEBUG_ENABLED
                    DM_OUT_P << "MIDI Channel Event: ";
#endif
                }
                thisMidiDeviceManager->slotParsePacket(packetArray);
            }
            else // sysex
            {
#ifdef MDM_DEBUG_ENABLED
                DM_OUT_P << "SysEx received";
 #endif
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
