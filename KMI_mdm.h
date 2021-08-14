// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef MIDIDEVICEMANAGER_H
#define MIDIDEVICEMANAGER_H

/* KMI Device Manager

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

#include "RtMidi.h"

//#include "WindowsMidiTypes.h"

//enum
//{
//    NORMAL,
//    BOOTLOADER_POST_UPDATE_REQUEST,
//    BOOTLOADER_NO_UPDATE_REQUEST
//};

enum
{
    NONE,
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

class MidiDeviceManager : public QWidget
{
    Q_OBJECT
public:
    explicit MidiDeviceManager(QWidget *parent = 0, int initPID = -1);

    // from device
    static void midiInCallback ( double deltatime, std::vector< unsigned char > *message, void *userData );

    // product ID
    int PID;
    int initPID;

    // RtMidi devices
    RtMidiIn *midi_in;
    RtMidiOut *midi_out;

    QString deviceName;

    QMap<int, QString> lookupPID;

    QByteArray devicebootloaderVersion;
    QByteArray deviceFirmwareVersion;
    QByteArray applicationFirmwareVersion;

    QFile *firmware;
    QByteArray firmwareByteArray;

//    QString expectedBootloaderVersion;
//    QString expectedFirmwareVersion;
//    QString foundBootloaderVersion;
//    QString foundFirmwwareVersion;

    //Helper variables to process sysex
    QByteArray sysExMessage; //Message to be processed;

    //Describes whether or not a fw update has been requested-- useful for managing bootloader reconnects
    bool fwUpdateRequested;
    bool inBootloader;

    bool queryReplied;

    QByteArray globals;
    QString    sysExType;
    bool       globalsRecieved;

    QTimer* versionPoller;
    bool versionReply;

    QString mode;

    bool ioGate;

    //------ Rx MIDI Data Variables
    uchar RPN_MSB[16];
    uchar RPN_LSB[16];
    uchar NRPN_MSB[16];
    uchar NRPN_LSB[16];
    uchar RPNorNRPN;

// public functions
    bool updatePortIn(int port);
    bool updatePortOut(int port);

signals:
    void signalFirmwareMismatch(MidiDeviceManager*);
    void signalFirmwareMatches(MidiDeviceManager*);

    void signalProgressDialog(QString messageType, int val);
    void signalFirmwareUpdateComplete();
    void signalConnected(bool);
    void signalFwBytesLeft(int);

    // SysEx messages
    void signalRxSysEx(QByteArray sysExMessageByteArray, std::vector< unsigned char > *message);

    // channel messages
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
    void signalStopPolling();

public slots:
    bool slotOpenMidiIn();
    bool slotOpenMidiOut();
    bool slotCloseMidiIn();
    bool slotCloseMidiOut();

    void slotSetExpectedFW(QByteArray fwVer);
    void slotPollVersion();
    void slotStartPolling();
    void slotStopPolling();

    void slotSendSysEx(unsigned char *sysEx, int len);
    void slotProcessSysEx(QByteArray sysExMessageByteArray, std::vector< unsigned char > *message);

    void slotUpdateFirmware();

    void slotParsePacket(QByteArray packetArray);
    void slotRxParam(int rpn, uchar val, uchar chan, uchar rpn_datatype);

private:
    int port_in, port_out;

};

#endif // MIDIDEVICEMANAGER_H
