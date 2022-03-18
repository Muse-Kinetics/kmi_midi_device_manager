// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef MIDIDEVICEMANAGER_H
#define MIDIDEVICEMANAGER_H

/* KMI MIDI Device Manager

  A cross-platform C++/Qt MIDI library for KMI devices.
  Written by Eric Bateman, August 2021.
  (c) Copyright 2021 Keith McMillen Instruments, all rights reserved.

  See KMI_dm.cpp for more details.

*/

#include <QDebug>
#include <QWidget>
#include <QtGui>
#include <QApplication>
#include <QTimer>
#include <QElapsedTimer>

#include "RtMidi.h"

enum
{
    NONE_PN,
    RPN,
    NRPN
};

enum PARAM_DATA_TYPES
{
    DATA_LSB,
    DATA_MSB,
    DATA_INC,
    DATA_DEC,

};

enum
{
    SIGNAL_NONE,
    SIGNAL_SEND
};



class MidiDeviceManager : public QWidget
{
    Q_OBJECT
public:
    explicit MidiDeviceManager(QWidget *parent = nullptr, int initPID = -1, QString objectNameInit = "undefined");

    // from device
    static void midiInCallback ( double deltatime, std::vector< unsigned char > *message, void *userData );

    // product ID
    int PID;
    int initPID;
    int PID_MIDI; // added for SoftStep to differentiate between rev1 and rev2

    // connection status based on if firmware matches. Firmware dialogs are handled before this is set true.
    bool connected;             // flag if device firmware matches and is connected
    bool port_in_open;          // flag if device input port is available and open
    bool port_out_open;         // flag if device output port is available and open

    int port_in, port_out;      // the current port in/out numbers

    bool restart;               // flag to halt all actions and restart the app

    // RtMidi devices
    RtMidiIn *midi_in;
    RtMidiOut *midi_out;

    // the name of the device detected by the PID
    QString deviceName;

    // the name of the object, used for debugging and reference
    QString objectName;

    QMap<int, QString> lookupPID;

    QByteArray devicebootloaderVersion;
    QByteArray deviceFirmwareVersion;
    QByteArray applicationFirmwareVersion;

    QFile *firmware;
    QByteArray firmwareByteArray;

    // for bootloader trojan horse firmware, ie softstep
    QFile *bootloader;
    QByteArray bootloaderByteArray;

    //Helper variables to process sysex
    QByteArray sysExMessage; //Message to be processed;

    //Describes whether or not a fw update has been requested-- useful for managing bootloader reconnects
    bool globalsRequested; // flag if we are storing global data to restore after update
    bool bootloaderMode;
    bool fwUpdateRequested;
    bool hackStopTimer;
    bool pollingStatus;

    QTimer* versionPoller;
    QTimer* timeoutFwBl;
    QTimer* timeoutGlobalsReq;

    QElapsedTimer versionReplyTimer;

    // counters to timeout timers
    unsigned char pollTimeout;
    int globalsTimerCount;

    // stops MIDI if sysex is sending
    bool ioGate;

    QDialog* errDialog;

    //------ Rx MIDI Data Variables
    uchar RPN_MSB[16];
    uchar RPN_LSB[16];
    uchar NRPN_MSB[16];
    uchar NRPN_LSB[16];
    uchar RPNorNRPN;

// public functions

    QByteArray decode8BitArray(QByteArray this8BitArray);

signals:
    // detect MIDI feedback loop
    void signalFeedbackLoopDetected(MidiDeviceManager*);

    // detect firmware version
    void signalFirmwareDetected(MidiDeviceManager*, bool);
    void signalFirmwareMismatch(QString, QString, QString);
    void signalStartPolling(QString);
    void signalStopPolling(QString);
    void signalStopGlobalTimer();
    void signalBootloaderMode(bool);
    void signalConnected(bool);

    // firmware update
    void signalFwConsoleMessage(QString message);
    void signalFwProgress(int thisPercent);
    void signalFirmwareUpdateComplete(bool);
    void signalFirmwareUpdateTimeout();
    void signalBeginBlTimer();
    void signalBeginFwTimer();

    void signalRequestGlobals();
    void signalRestoreGlobals();

    // SysEx messages
    void signalRxSysExBA(QByteArray sysExMessageByteArray);
    void signalRxSysEx(std::vector< unsigned char > *message);

    // channel messages
    void signalRxMidi_raw(uchar status, uchar d1, uchar d2, uchar chan);
    void signalRxMidi_noteOff(uchar chan, uchar note, uchar velocity);
    void signalRxMidi_noteOn(uchar chan, uchar note, uchar velocity);
    void signalRxMidi_polyAT(uchar chan, uchar note, uchar val);
    void signalRxMidi_controlChange(uchar chan, uchar cc, uchar val);
    void signalRxMidi_RPN(uchar chan, int rpn, int val, uchar messagetype);
    void signalRxMidi_NRPN(uchar chan, int rpn, int val, uchar messagetype);
    void signalRxMidi_progChange(uchar chan, uchar val);
    void signalRxMidi_aftertouch(uchar chan, uchar val);
    void signalRxMidi_pitchBend(uchar chan, int val);

    // system common messages
    void signalRxMidi_MTC(uchar d1, uchar d2);
    void signalRxMidi_SongPosition(uchar LSB, uchar MSB);
    void signalRxMidi_SongSelect(uchar song);
    void signalRxMidi_TuneReq();
    void signalRxMidi_Clock();
    void signalRxMidi_Start();
    void signalRxMidi_Continue();
    void signalRxMidi_Stop();
    void signalRxMidi_ActSense();
    void signalRxMidi_SysReset();

public slots:

    void slotUpdatePID(int thisPID);
    bool slotUpdatePortIn(int port);
    bool slotUpdatePortOut(int port);
    bool slotOpenMidiIn();
    bool slotOpenMidiOut();
    bool slotCloseMidiIn(bool);
    bool slotCloseMidiOut(bool);

#ifndef Q_OS_WIN
    bool slotCreateVirtualIn(QString portName);
    bool slotCreateVirtualOut(QString portName);
#endif

    void slotResetConnections(QString portNameApp, QString portNameBootloader);

    void slotTestFeedbackLoop();

    void slotSetExpectedFW(QByteArray fwVer);
    void slotPollVersion();
    void slotStartPolling(QString Caller);
    void slotStopPolling(QString caller);
    void slotStartGlobalsTimer();
    void slotStopGlobalTimer();
    void slotCheckGlobalsReceived();

    void slotSendSysExBA(QByteArray thisSysexArray); // convert bytearray to unsigned char pointer
    void slotSendSysEx(unsigned char *sysEx, int len); // takes a pointer and the size of the array
    void slotProcessSysEx(QByteArray sysExMessageByteArray, std::vector< unsigned char > *message);

    // firmware update
    bool slotOpenFirmwareFile(QString filePath);
    bool slotOpenBootloaderFile(QString filePath);
    void slotRequestFirmwareUpdate();
    void slotEnterBootloader();
    void slotBeginBlTimer();
    void slotBootloaderTimeout();
    void slotUpdateFirmware();
    void slotBeginFwTimer();
    void slotFirmwareTimeout();
    void slotFirmwareUpdateReset();

    // four prototypes for various packet sizes
    void slotSendMIDI(uchar status);
    void slotSendMIDI(uchar status, uchar d1);
    void slotSendMIDI(uchar status, uchar d1, uchar d2);
    void slotSendMIDI(uchar status, uchar d1, uchar d2, uchar chan);

    void slotParsePacket(QByteArray packetArray);
    void slotRxParam(int rpn, uchar val, uchar chan, uchar rpn_datatype);

    void slotErrorPopup(QString errorMessage);

private:
    bool callbackIsSet;

};

#endif // MIDIDEVICEMANAGER_H
