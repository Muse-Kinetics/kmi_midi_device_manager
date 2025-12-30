// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef KMI_PORTS_H
#define KMI_PORTS_H

/* KMI Ports

  A cross-platform C++/Qt MIDI library for KMI devices.
  Written by Eric Bateman, August 2021.
  (c) Copyright 2021 Keith McMillen Instruments, all rights reserved.

  See kmi_ports.cpp for more details.

*/

#include "RtMidi.h"
#include <QtWidgets>
#include <QTimer>

enum
{
    PORT_CONNECT,
    PORT_DISCONNECT,
    PORT_CHANGED
};

enum
{
    PORT_IN,
    PORT_OUT,
};

extern int lastMIDIIN_QuNexus, lastMIDIOUT_QuNexus;

QString portNameFix(QString); // strip port number from end of RtMidi windows port names

class KMI_Ports : public QWidget
{
    Q_OBJECT

public:

    QWidget *thisParent;
    QStringList inOut;
    QStringList mType;

    explicit KMI_Ports(QWidget *parent = nullptr);

    // public variables
    int numInputs, numOutputs;

    // MIDI handlers to manage input and output ports
    RtMidiIn *IN_PORT_MGR;
    RtMidiOut *OUT_PORT_MGR;

#ifndef Q_OS_WIN
    // Virtual ports
    RtMidiIn *IN_PORT_VIRTUAL;
    RtMidiOut *OUT_PORT_VIRTUAL;
#endif


    // These maps contain the last known MIDI port configuration
    // Iterate through them regularly and verify by port name
    QMap<QString, int> midiInputPorts;
    QMap<QString, int> midiOutputPorts;

    //------------------- Polling
    QTimer* devicePoller;

    // pubic functions
    int getInPortNumber(QString);
    int getOutPortNumber(QString);
    QString getInPortName(int thisPort);
    QString getOutPortName(int thisPort);
    int checkPortsForChanges();          // checks for MIDI i/o changes
    void listMaps();

signals:

    // portUpdated is emitted anytime the system MIDI ports change.
    // It reports the name of the port, and if it was added/deleted or
    // if it's port # changed
    void signalPortUpdated(QString, uchar, uchar, int);

    void signalInputCount(int);
    void signalInputPort(QString, int);
    void signalOutputCount(int);
    void signalOutputPort(QString, int);
    void signalClearPortMaps();

public slots:

    void slotPollDevices();         // call pollPorts for input/output
    void slotRefreshPortMaps();         // force refresh

#ifndef Q_OS_WIN
    void slotCreateVirtualIn(QString portName);
    void slotCreateVirtualOut(QString portName);
    void slotCloseVirtualIn();
    void slotCloseVirtualOut();
#endif


};


#endif // KMI_PORTS_H
