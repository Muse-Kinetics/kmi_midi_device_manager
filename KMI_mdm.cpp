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
#include "kmi_ports.h"
#include <QMessageBox>

//#define MDM_DEBUG_ENABLED 1

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
    lookupPID.insert(PID_SOFTSTEP2_OLD, "SSCOM");
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

    globalsTimerCount = 0;
    pollTimeout = 0;


    // flags
    connected = false;
    restart = false;
    ioGate = true; // open gate
    port_in_open = false;
    port_out_open = false;
    callbackIsSet = false;

    firstFwResponseReceived = false;

    globalsRequested = false;
    bootloaderMode = false;
    fwUpdateRequested = false;
    hackStopTimer = false;
    pollingStatus = false;



    // setup RtMidi connections
    //midi_in = new RtMidiIn();
    //midi_out = new RtMidiOut();
    midi_in = nullptr;
    midi_out = nullptr;

    // firmware and bootloader timeout timer
    //timeoutFwBl = new QTimer(this);

    versionPoller = new QTimer(this);

    versionPoller->start(5000); // start the timer

    versionReplyTimer.start();

    // timers have to be triggered by these signals from the main thread
    //connect(this, SIGNAL(signalStartPolling(QString)), this, SLOT(slotStartPolling(QString)));
    //connect(this, SIGNAL(signalStopPolling(QString)), this, SLOT(slotStopPolling(QString)));
    connect(this, SIGNAL(signalBeginBlTimer()), this, SLOT(slotBeginBlTimer()));
    connect(this, SIGNAL(signalBeginFwTimer()), this, SLOT(slotBeginFwTimer()));
    connect(this, SIGNAL(signalStopGlobalTimer()), this, SLOT(slotStopGlobalTimer()));
}

void MidiDeviceManager::slotUpdatePID(int thisPID)
{
    PID = thisPID;
    deviceName = lookupPID.value(PID);
    //qDebug() << "updatePID: " << PID << " deviceName: " << deviceName;
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

    midi_in = new RtMidiIn(); // refresh RtMidi

    try
    {

        // setup RtMidi connections

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
        port_in_open = false;
        portName_in = "";
        connected = false;
        bootloaderMode = false;
        delete midi_in;
        midi_in = nullptr;
        return 0;
    }

    portName_in = portNameFix(QString::fromStdString(midi_in->getPortName(port_in)));
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

    midi_out = new RtMidiOut(); // refresh instance

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
        delete midi_out;
        midi_out = nullptr;
        return 0;
    }
    portName_out = portNameFix(QString::fromStdString(midi_out->getPortName(port_out)));
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
            midi_in->cancelCallback();
            callbackIsSet = false;
        }
        DM_OUT << "deleting midi_in";
        delete midi_in;
        midi_in = nullptr;
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
        DM_OUT << "deleting midi_out";
        delete midi_out;
        midi_out = nullptr;
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

        midi_in = new RtMidiIn(); // refresh RtMidi

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
        delete midi_in;
        midi_in = nullptr;
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

        midi_out = new RtMidiOut(); // refresh instance

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
        delete midi_out;
        midi_out = nullptr;
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
void MidiDeviceManager::slotResetConnections(QString portNameApp, QString portNameBootloader)
{
    DM_OUT << "slotResetConnections called - portName: " << portNameApp << " portNameBootloader: " << portNameBootloader << "bootloaderMode: " << bootloaderMode;
    bool refreshDone = false;
    bool initialBootloaderMode = bootloaderMode;

    QString thisPortName;

#ifdef Q_OS_WIN
    QString lastPortName = portNameFix(QString::fromStdString(midi_in->getPortName(port_in)));
#else
    QString lastPortName = QString::fromStdString(midi_in->getPortName(port_in));
#endif
    unsigned char numInPorts, numOutPorts;

    if (midi_in != NULL)
    {
        midi_in->cancelCallback();
    }
    else
    {
        DM_OUT << "WARNING: midi_in does not exist, aborting cancel callback";
    }

    pollingStatus = false;

    DM_OUT << "Closing ports..." << lastPortName;

    //close ports
    midi_in->closePort();
    midi_out->closePort();

//    DM_OUT << "deleting ports...";
//    delete midi_out;
//    delete midi_in;

    DM_OUT << "new rtmidi instances...";
    midi_in = new RtMidiIn(); // refresh instance
    midi_out = new RtMidiOut(); // refresh instance

    while (!refreshDone) // loop until the port changes
    {  
        // this is a hack for now, KMI_Ports should be reporting these changes, investigate this further
        DM_OUT << "slotResetConnections - getPortCount";
        numInPorts = midi_in->getPortCount();
        numOutPorts = midi_out->getPortCount();

        // test/fix input port name
        for (uint thisPort = 0; thisPort < (numInPorts); thisPort++)
        {
            if (thisPort <= numInPorts)
            {  

                QString newPortName = portNameFix(QString::fromStdString(midi_in->getPortName(thisPort)));

                DM_OUT << "find in port - thisPort: " << thisPort << " newPortName: " << newPortName;

                // confirm we are either going bootLoader->app or app->bootLoader
                if ((initialBootloaderMode && newPortName == portNameApp) || (!initialBootloaderMode && newPortName == portNameBootloader))
                {
                    DM_OUT << "Input port match - initialBootloaderMode: " << initialBootloaderMode << " newPortName: " << newPortName;
                    port_in = thisPort;
                }
            }
            else
            {
                DM_OUT << "Port subtraction before end of for loop, numInPorts: " << numInPorts << " thisPort: " << thisPort;
            }
        }

        // test/fix output port name
        for (uint thisPort = 0; thisPort < (numOutPorts); thisPort++)
        {
            if (thisPort <= numOutPorts)
            {
                QString newPortName = portNameFix(QString::fromStdString(midi_out->getPortName(thisPort)));

                DM_OUT << "find out port - thisPort: " << thisPort << " newPortName: " << newPortName;

                // confirm we are either going bootLoader->app or app->bootLoader
                if ((initialBootloaderMode && newPortName == portNameApp) || (!initialBootloaderMode && newPortName == portNameBootloader))
                {
                    DM_OUT << "Output port match - initialBootloaderMode: " << initialBootloaderMode << " newPortName: " << newPortName;
                    port_out = thisPort;
                }
            }
            else
            {
                DM_OUT << "Port subtraction before end of for loop, numOutPorts: " << numOutPorts << " thisPort: " << thisPort;
            }
        }

        // attempting to open the ports when we are in between app and bootloader modes helps to flush out the old port settings
        try
        {
            thisPortName = portNameFix(QString::fromStdString(midi_in->getPortName(port_in)));

            DM_OUT << "Opening ports..." << thisPortName;

            midi_in->openPort(port_in);
            midi_out->openPort(port_out);
        }
        catch (RtMidiError &error)
        {
            /* Return the error */
            thisPortName = ""; // prevents refreshDone being set to true in next if statement
            DM_OUT << "Port #" << port_in << " not found, retrying. Error Message: " << QString::fromStdString(error.getMessage());

        }

        // *****************************************
        // validate if the ports have been refreshed
        // *****************************************
        if (!initialBootloaderMode && thisPortName == portNameBootloader) // test for app->bootloader
        {
            DM_OUT << "app -> bootloader, thisPortName: " << thisPortName << " portNameBootloader: " << portNameBootloader;

            if (thisPortName == "SoftStep Bootloader Port 1") slotUpdatePID(PID_SOFTSTEP); // hack for softstep

            refreshDone = true;
        }
        else if (initialBootloaderMode && thisPortName == portNameApp) // test if bootloader->app
        {   
            DM_OUT << "bootloader -> app, ports match - thisPortName: " << thisPortName << " portNameApp: " << portNameApp;
            refreshDone = true;
        }
        else // we are about to loop back - close the ports, which is necessary to flush out the old settings
        {
            try
            {
                DM_OUT << "No port match - initialBootloaderMode: " << initialBootloaderMode << " thisPortName: " << thisPortName;

                DM_OUT << "Closing ports..." << thisPortName;
                midi_in->closePort();
                midi_out->closePort();

                DM_OUT << "Re-creating midi_in and midi_out instances...";
                midi_in = new RtMidiIn(); // refresh instance
                midi_out = new RtMidiOut(); // refresh instance
            }
            catch (RtMidiError &error)
            {
                /* Return the error */
                DM_OUT << "Port still not found, retrying. Error Message: " << QString::fromStdString(error.getMessage());
            }
        }

        QThread::sleep(1);
    }

// brute force, but necessary to reset the ports
#ifdef Q_OS_WIN
    // restart:

    restart = true;

    QMessageBox msgBox;
    if (initialBootloaderMode) // if we are going bootloader->app
    {
        emit signalFwProgress(100);

        msgBox.setText(QString("Firmware update sent.\n\nThe application must now re-start to refresh the MIDI driver and connect to the %1.").arg(deviceName));
    }
    else // app->bootloader
    {
        msgBox.setText("Bootloader command sent.\n\nThe application must now re-start to refresh the MIDI driver.");
    }
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();

    qApp->quit();
    QProcess::startDetached(qApp->arguments()[0], qApp->arguments());
#endif

    DM_OUT << "slotResetConnections set callback";
    midi_in->ignoreTypes( false, false, false );

    midi_in->cancelCallback();
    QThread::sleep(1);
    midi_in->setCallback( &MidiDeviceManager::midiInCallback, this);

    if (initialBootloaderMode)
    {
        //slotStartPolling("slotResetConnections - bootloader->app successful"); // bootloader->app, begin polling
        pollingStatus = true;
    }
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
void MidiDeviceManager::slotStartPolling(QString caller)
{
    DM_OUT << "slotStartPolling called - caller:" << caller << " pollingStatus:" << pollingStatus;

    slotStopPolling("slotStartPolling clearing before enable");

    int pollTime;

    //versionPoller = new QTimer(this);
    connect(versionPoller, SIGNAL(timeout()), this, SLOT(slotPollVersion()));

    // some devices take longer to boot up
    if (deviceName == "12 Step")
    {
        pollTime = 4000;
    }
    else if (deviceName == "SSCOM" || deviceName == "SoftStep")
    {
        pollTime = 4000;
    }
    else
    {
        pollTime = 500;
    }
    //versionPoller->start(pollTime);
    versionPoller->setInterval(pollTime);

    pollingStatus = true; // allow polling to proceed
}

void MidiDeviceManager::slotStopPolling(QString caller) // this has to be called from an emitted signal so that it's handled in the main thread
{
    DM_OUT << "slotStopPolling called - caller:" << caller;
    //versionPoller->stop();

    disconnect(this, SLOT(slotPollVersion())); // disconnect all slots/signals for the timer
    pollingStatus = false; // block polling
}

void MidiDeviceManager::slotPollVersion()
{
    if (pollingStatus == false) return; // avoid starting the timer multiple times

    DM_OUT << "slotPollVersion called - pollingStatus: " << pollingStatus << " in_open: " << port_in_open << "in port#: " << port_in <<  " out_open: " << port_out_open << " port_out: " << port_out;

    if (midi_in == nullptr)
    {
        DM_OUT << "ERROR: midi_in is not instantiated, aborting slotPollVersion!";
        return; // handler doesn't exist
    }

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
            //emit signalStopPolling("slotPollVersion");
            pollingStatus = false;
            return;
        }
        pollTimeout++;

    }
    else
    {
        // ports are setup, make sure callback is correctly set
        midi_in->cancelCallback();

//        slotCloseMidiIn();
//        slotCloseMidiOut();
//        slotOpenMidiIn();
//        slotOpenMidiOut();
//        updatePortIn(port_in);
//        updatePortOut(port_out);

        midi_in->setCallback( &MidiDeviceManager::midiInCallback, this);
    }

    if (hackStopTimer)
    {
        //emit signalStopPolling("slotPollVersion - hackStopTimer");
        pollingStatus = false;
        hackStopTimer = false;
        return;
    }

    if (deviceName == "SSCOM") // old softStep firmware doesn't use the universal syx dev id request
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
    DM_OUT << "Send sysex, length: " << len << " syx: " << sysEx << " PID: " << PID;
    std::vector<unsigned char> message(sysEx, sysEx+len);

    ioGate = false; // pause any midi output while sending SysEx

    if (midi_out == nullptr)
    {
        DM_OUT << "ERROR: midi_out is not instantiated, aborting slotSendMIDI!";
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
        slotCloseMidiIn(SIGNAL_SEND); // better than letting the app crash? Only if we alert the end user, otherwise this becomes a support
        slotCloseMidiOut(SIGNAL_SEND); // better than letting the app crash? Only if we alert the end user, otherwise this becomes a support
        emit signalFeedbackLoopDetected(this);
        slotErrorPopup("MIDI FEEDBACK LOOP DETECTED\nPorts Closed");
    }

    // Query for "pre-bootloader" SoftStep replies
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
        pollingStatus = false; // turn off polling

        // this is the old softstep reply
        DM_OUT << "SoftStep fw reply:" <<  sysExMessageByteArray;

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

        if ((unsigned char)sysExMessageByteArray.at(9) == 1)
        {
            bootloaderMode = true;
        }
        else
        {
            bootloaderMode = false;
        }

        devicebootloaderVersion[0] = sysExMessageByteArray[12];
        devicebootloaderVersion[1] = sysExMessageByteArray[13];
        devicebootloaderVersion[2] = sysExMessageByteArray[14];

        deviceFirmwareVersion[0] = sysExMessageByteArray[15];
        deviceFirmwareVersion[1] = sysExMessageByteArray[16];
        deviceFirmwareVersion[2] = sysExMessageByteArray[17];

        PID_MIDI = (uchar)sysExMessageCharArray->at(8); // store the MIDI PID - added for SoftStep to differentiate version 1 vs 2

        DM_OUT << "ID Reply - PID_MIDI: " << PID_MIDI << " BL: " << devicebootloaderVersion << " FW: " << deviceFirmwareVersion; // << " fullMsg: " << sysExMessageByteArray;
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

        //DM_OUT << "passing SysEx to applicaiton";
        // send SysEx to application
        emit signalRxSysExBA(sysExMessageByteArray);
        emit signalRxSysEx(sysExMessageCharArray);

        // leave function
        return;
    }


    // only allow 1 version response every 5 seconds
    if (!versionReplyTimer.hasExpired(5000) && firstFwResponseReceived == true) return;

    firstFwResponseReceived = true; // now we can use the timer
    versionReplyTimer.restart();

    // process firmware version connection messages
    //emit signalStopPolling("slotProcessSysEx");
    pollingStatus = false;

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
            //emit signalStopPolling("slotProcessSysEx - fw match");
            pollingStatus = false;
            hackStopTimer = true;
            if (globalsRequested)
            {
                globalsRequested = false;
                emit signalRestoreGlobals();
                emit signalFwProgress(90); // increment progress bar
                emit signalFwConsoleMessage("\nRestoring Globals...");
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
        DM_OUT << "emit fw mismatch - fwv: " << deviceFirmwareVersion.toUInt() << "cfwv: " << applicationFirmwareVersion.toUInt();
        emit signalFirmwareDetected(this, false);
        //emit signalFirmwareMismatch(QString(devicebootloaderVersion), QString(applicationFirmwareVersion), QString(deviceFirmwareVersion));
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
                //emit signalStopPolling("slotRequestFirmwareUpdate - globals received, enter bootloader");
                pollingStatus = false;

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
                emit signalFwConsoleMessage(QString("\nBacking up %1 global settings...").arg(deviceName));
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
    DM_OUT << "slotEnterBootloader called";

    // TODO - this works for qunexus, add code for other controllers
    switch (PID)
    {
    case PID_SOFTSTEP:
    case PID_SOFTSTEP2_OLD:
        if ((uchar)deviceFirmwareVersion[0] < 1) // pre-bootloader firmware
        {
            // version 98 is a placeholder for ZenDesk users and has a bootloader
            // version 99 is the trojan horse bootloader update
            // everything else needs to have the bootloader installed
            if (((uchar)deviceFirmwareVersion[0] == 9 && (uchar)deviceFirmwareVersion[1] < 8) ||
                 (uchar)deviceFirmwareVersion[0] < 9)
            {
                emit signalFwConsoleMessage("\n*** Installing bootloader *** - device will reboot several times!\n");

                // this will install the firmware and reboot the device into bootloader mode
                slotSendSysExBA(bootloaderByteArray);
            }
        }
        else // this should be the standard method moving forward
        {
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

    if (PID != PID_SOFTSTEP && PID != PID_SOFTSTEP2_OLD)
    {
#ifdef Q_OS_WIN
        emit signalFwConsoleMessage("\nSending Enter bootloader Command, device and application will reboot.\n");
#else
        emit signalFwConsoleMessage("\nSending Enter bootloader Command, device will reboot.\n");
#endif
    }

    emit signalFwProgress(20); // increment progress bar

    DM_OUT << "BL timeout counter started";
    emit signalBeginBlTimer();
}

void MidiDeviceManager::slotBeginBlTimer()
{
    DM_OUT << "slotBeginBlTimer called";
    int timeoutTime;

    switch (PID)
    {
#ifdef Q_OS_WIN
    case PID_QUNEXUS:
        timeoutTime = 10000;
        break;
#endif
    case PID_QUNEO: // need extra time for QuNeo
        timeoutTime = 5000;
        break;
    default:
        timeoutTime = 3000;
    }

//#ifndef Q_OS_WIN
    QTimer::singleShot(timeoutTime, this, SLOT(slotBootloaderTimeout()));
//#endif
}

void MidiDeviceManager::slotBootloaderTimeout()
{
    DM_OUT << "slotBootloaderTimeout called - bootloaderMode: " << bootloaderMode;
    if (bootloaderMode) return;
    emit signalFwConsoleMessage(QString("\nPinging %1 for bootloader status...\n").arg(deviceName));
    //emit signalStartPolling("slotBootloaderTimeout"); // begin polling
    pollingStatus = true;
}

void MidiDeviceManager::slotUpdateFirmware()
{
    DM_OUT << "slotUpdateFirmware called - port_out_open: " << port_out_open << " fw .syx size: " << firmwareByteArray.length();
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

    DM_OUT << "sending firmware sysex";

    slotSendSysExBA(firmwareByteArray); // send firmware

    DM_OUT << "FW timeout counter started";
    emit signalBeginFwTimer();
}

void MidiDeviceManager::slotBeginFwTimer()
{
    DM_OUT << "slotBeginFwTimer called";
#ifndef Q_OS_WIN
    int timeoutTime;

    switch (PID)
    {
    case PID_12STEP: // need extra time for QuNeo
        timeoutTime = 20000;
        break;
    default:
        timeoutTime = 15000;
    }
    QTimer::singleShot(timeoutTime, this, SLOT(slotFirmwareTimeout()));
#endif
}

void MidiDeviceManager::slotFirmwareTimeout()
{
    DM_OUT << "slotFirmwareTimeout called - fwUpdateRequested: " << fwUpdateRequested;
    if (!fwUpdateRequested) return;

    emit signalFwProgress(75); // increment progress bar
    emit signalFwConsoleMessage("\nPinging Device for version info...\n");

    //emit signalStartPolling("slotFirmwareTimeout"); // begin polling
    // EB TODO - verify that this change doesn't mess up QuNexus, QuNeo, Softstep etc
    pollingStatus = true; // re-enable polling
}

void MidiDeviceManager::slotFirmwareUpdateReset()
{
    DM_OUT << "slotFirmwareUpdateReset called";
    fwUpdateRequested = false;
    bootloaderMode = false;
    globalsRequested = false;
    hackStopTimer = false;
    //emit signalStopPolling("slotFirmwareUpdateReset");
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

#ifdef MDM_DEBUG_ENABLED
    DM_OUT << QString("slotSendMIDI called - status: %1 d1: %2 d2: %3 channel: %4").arg(status).arg(d1).arg(d2).arg(chan);
#endif

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

    if (midi_out == nullptr)
    {
        DM_OUT << "ERROR: midi_out is not instantiated, aborting slotSendMIDI!";
        return; // handler doesn't exist
    }

    // prepare and send the packet
    try
    {
        midi_out->sendMessage( &packet );
    }
    catch (RtMidiError &error)
    {
        DM_OUT << "MIDI SEND ERR:" << (QString::fromStdString(error.getMessage()));
        emit signalFwConsoleMessage("MIDI SEND ERR:" + (QString::fromStdString(error.getMessage())));
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
