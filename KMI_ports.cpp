// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
/* KMI Ports

  A cross-platform C++/Qt MIDI library for KMI devices.
  Written by Eric Bateman, August 2021.
  (c) Copyright 2021 Keith McMillen Instruments, all rights reserved.

  Features:
  - Detect and report changes to OS MIDI ports. This should be managed separately from the MIDI device manager objects that
  an application uses, as some applications may need access to multiple ports/devices.

  Uses RtMidi

*/

#include "kmi_ports.h"
#include "KMI_DevData.h"

#define qsFromStd QString::fromStdString

int lastMIDIIN_QuNexus = 0; // for portNameFix on Windows
int lastMIDIOUT_QuNexus = 0;

KMI_Ports::KMI_Ports(QWidget *Parent)
{
    qDebug() << "KMI_Ports instance created";

    thisParent = Parent; // suppress warning

    // initialize input counters
    numInputs = numOutputs = 0;

    IN_PORT_MGR = new RtMidiIn();
    OUT_PORT_MGR = new RtMidiOut();

#ifndef Q_OS_WIN
    // Virtual ports
    IN_PORT_VIRTUAL = new RtMidiIn();
    OUT_PORT_VIRTUAL = new RtMidiOut();
#endif

    //setup timer for regularly polling ports
    devicePoller = new QTimer(this);
    connect(devicePoller, SIGNAL(timeout()), this, SLOT(slotPollDevices()));
    //devicePoller->start(1);

    // debug enum translations
    inOut <<
            "IN" <<
            "OUT";

    mType <<
             "CONNECT"<<
             "DISCONNECT"<<
             "CHANGED";
}

// ****************************
// Public Slots
// ****************************


void KMI_Ports::slotPollDevices()
{
//    int newInputCount = 0;
//    int newOutputCount = 0;

    //qDebug() << "slotPollDevices called - delete/recreate active RtMidi instances";

    if (numInputs) // don't re-initialize RtMidi if there are no inputs
    {
        delete IN_PORT_MGR;
        IN_PORT_MGR = nullptr;
        IN_PORT_MGR = new RtMidiIn();
    }
    if (numOutputs)
    {
        delete OUT_PORT_MGR;
        OUT_PORT_MGR = nullptr;
        OUT_PORT_MGR = new RtMidiOut();
    }

    // Previous method would count the ports and compare to the previous count, but this can miss
    // changes in between slotPollDevices intervals if the number of new ports matches the old
//    try
//    {
//        newInputCount = IN_PORT_MGR->getPortCount();
//    }
//    catch (RtMidiError &error)
//    {
//        /* Return the error */
//        qDebug() << "RtMidi Error:" << (QString::fromStdString(error.getMessage()));
//    }

//    try
//    {
//        newOutputCount = OUT_PORT_MGR->getPortCount();
//    }
//    catch (RtMidiError &error)
//    {
//        /* Return the error */
//        qDebug() << "RtMidi Error:" << (QString::fromStdString(error.getMessage()));
//    }

//    qDebug() << "slotPollDevices - newIn: " << newInputCount << " prevIn: " << numInputs << " - newOut: " << newOutputCount << " prevOut: " << numOutputs;
//    if (newInputCount != numInputs || newOutputCount != numOutputs)
//    {
//        qDebug() << "KMI_Ports: slotPollDevices mismatch!";
        if(checkPortsForChanges())
        {
            listMaps();
        }
    //}
}

// force a refresh by clearing the port metadata
void KMI_Ports::slotRefreshPortMaps()
{
    emit signalClearPortMaps();
    numInputs = 0;
    numOutputs = 0;
    midiInputPorts.clear();
    midiOutputPorts.clear();

    qDebug() << "************ all ports disconnected, listing ports **************";
    listMaps();
}

// ****************************
// Public Functions (not slots)
// ****************************

// Function that polls system MIDI devices looking for changes. Windows
// does not report changes automatically, so we must ping constantly to detect them. We can use
// this for MacOS as well to have consistent behavior and streamlined development.
//
// We need to scan inputs and outputs at the same time and first report deletions, then additions, then changes
int KMI_Ports::checkPortsForChanges() // returns the number of changed ports (additions, subtractions, port renumbers)
{
    int returnValue = 0;

    //if (IN_PORT_MGR == nullptr || OUT_PORT_MGR == nullptr) return 0; // safety check

    //qDebug() << "++ pollPorts called - inOrOut: " << inOut[inOrOut] << "\n";

    // update map pointer
    QMap<QString, int> *globalInputPortMap = &midiInputPorts;
    QMap<QString, int> *globalOutputPortMap = &midiOutputPorts;

    QMap<QString, int> newInputPortMap;
    QMap<QString, int> newOutputPortMap;

    newInputPortMap.clear(); // ensure the map is empty
    newOutputPortMap.clear(); // ensure the map is empty

    unsigned int numInPorts = IN_PORT_MGR->getPortCount();
    unsigned int numOutPorts = OUT_PORT_MGR->getPortCount();

    //qDebug() << "Load newInputPortMap";
    // load newInputPortMap with the current RtMidi port list
    for (uint thisPort = 0; thisPort < (numInPorts); thisPort++)
    {
        QString newPortName = getInPortName(thisPort);

        if (newPortName != "None" && newPortName != "")
        {
            newInputPortMap.insert(newPortName, thisPort); // add this to our new map
        }
    }

    //qDebug() << "Load newOutputPortMap";
    // load newOutputPortMap with the current RtMidi port list
    for (uint thisPort = 0; thisPort < (numOutPorts); thisPort++)
    {
        QString newPortName = getOutPortName(thisPort);

        if (newPortName != "None" && newPortName != "")
        {
            newOutputPortMap.insert(newPortName, thisPort); // add this to our new map
        }
    }

    // *************************************************************************************************************
    // DELETE INPUTS - compare globalInputPortMap to newInputPortMap and delete ports that have been disconnected
    // *************************************************************************************************************
    //qDebug() << "Delete missing inputs";
    QMap<QString, int>::iterator i = globalInputPortMap->begin();
    while (i != globalInputPortMap->end())
    {
        // get the key/value pair
        QString globalInputPortMapName = i.key();
        int globalMapPort = i.value();

        // check if this port has been deleted
        if (!newInputPortMap.contains(globalInputPortMapName))
        {
            //qDebug() << "****** Input Port delete " << globalInputPortMapName << "\n";
            // tell main app to disconnect port
            emit signalPortUpdated(globalInputPortMapName, PORT_IN, PORT_DISCONNECT, globalMapPort);

            // update our global port map
            i = globalInputPortMap->erase(i); // erases this entry

            returnValue++; // increase changed port count to return;
        }
        else
        {
            i++;
        }
    }

    // *************************************************************************************************************
    // DELETE OUTPUTS - compare globalOutputPortMap to newOutputPortMap and delete ports that have been disconnected
    // *************************************************************************************************************
    //qDebug() << "Delete missing outputs";
    i = globalOutputPortMap->begin();
    while (i != globalOutputPortMap->end())
    {
        // get the key/value pair
        QString globalOutputPortMapName = i.key();
        int globalMapPort = i.value();

        // check if this port has been deleted
        if (!newOutputPortMap.contains(globalOutputPortMapName))
        {
            //qDebug() << "****** Output Port delete " << globalOutputPortMapName << "\n";
            // tell main app to disconnect port
            emit signalPortUpdated(globalOutputPortMapName, PORT_OUT, PORT_DISCONNECT, globalMapPort);

            // update our global port map
            i = globalOutputPortMap->erase(i); // erases this entry

            returnValue++; // increase changed port count to return;
        }
        else
        {
            i++;
        }
    }

    // *************************************************************************************************************
    // FIND INPUTS - Search newInputPortMap and find new ports that aren't in globalInputPortMap and add/connect them
    // *************************************************************************************************************
    //qDebug() << "Find new inputs";
    i = newInputPortMap.begin();
    while (i != newInputPortMap.end())
    {
        // get the key/value pair
        QString newInputPortMapName = i.key();
        int newMapPort = i.value();

        // check if this port is new
        if (!globalInputPortMap->contains(newInputPortMapName))
        {
            //qDebug() << "****** Input Port add " << newInputPortMapName << "\n";
            // alert main app
            emit signalPortUpdated(newInputPortMapName, PORT_IN, PORT_CONNECT, newMapPort);

            // update the global i/o map
            globalInputPortMap->insert(newInputPortMapName, newMapPort); // increments i

            returnValue++; // increase changed port count to return;
        }
        else
        {
            i++;
        }
    }

    // *************************************************************************************************************
    // FIND OUTPUTS - Search newOutputPortMap and find new ports that aren't in globalOutputPortMap and add/connect them
    // *************************************************************************************************************
    //qDebug() << "Find new outputs";
    i = newOutputPortMap.begin();
    while (i != newOutputPortMap.end())
    {
        // get the key/value pair
        QString newOutputPortMapName = i.key();
        int newMapPort = i.value();

        // check if this port is new
        if (!globalOutputPortMap->contains(newOutputPortMapName))
        {
            //qDebug() << "****** Output Port add " << newOutputPortMapName << "\n";

            // alert main app
            emit signalPortUpdated(newOutputPortMapName, PORT_OUT, PORT_CONNECT, newMapPort);

            // update the global i/o map
            globalOutputPortMap->insert(newOutputPortMapName, newMapPort);  // increments i

            returnValue++; // increase changed port count to return;
        }
        else
        {
            i++;
        }
    }

    // *************************************************************************************************************
    // FIND CHANGED INPUTS - Compare globalInputPortMap to newInputPortMap and find changed port numbers
    // *************************************************************************************************************
    //qDebug() << "Find changed inputs";
    i = globalInputPortMap->begin();
    while (i != globalInputPortMap->end())
    {
        // get the old key/value pair
        QString globalInputPortMapName = i.key();
        int globalMapPort = i.value();

        // get the new port number
        int updatedPort = newInputPortMap.value(globalInputPortMapName);

        // check if the port # has changed
        if (updatedPort != globalMapPort)
        {
            // update our portmap
            globalInputPortMap->insert(globalInputPortMapName, updatedPort);

            // tell main app to update port
            emit signalPortUpdated(globalInputPortMapName, PORT_IN, PORT_CHANGED, updatedPort);

            returnValue++; // increase changed port count to return;
        }
        i++; // increment

    }

    // *************************************************************************************************************
    // FIND CHANGED OUTPUTS - Compare globalOutputPortMap to newOutputPortMap and find changed port numbers
    // *************************************************************************************************************
    //qDebug() << "Find changed outputs";
    i = globalOutputPortMap->begin();
    while (i != globalOutputPortMap->end())
    {
        // get the key/value pair
        QString globalOutputPortMapName = i.key();
        int globalMapPort = i.value();

        // get the new port number
        int updatedPort = newOutputPortMap.value(globalOutputPortMapName);

        // check if the port # has changed
        if (updatedPort != globalMapPort)
        {
            // update our portmap
            globalOutputPortMap->insert(globalOutputPortMapName, updatedPort);

            // tell main app to update port
            emit signalPortUpdated(globalOutputPortMapName, PORT_OUT, PORT_CHANGED, updatedPort);

            returnValue++; // increase changed port count to return;
        }
        i++; // increment
    }
    return returnValue;
}

int KMI_Ports::getInPortNumber (QString thisPortName)
{
    if (IN_PORT_MGR == nullptr || OUT_PORT_MGR == nullptr) return -1; // safety check

    //qDebug() << "getInPortNumber called: " << thisPortName;
    unsigned int numInPorts = IN_PORT_MGR->getPortCount();
    // find the matching port name and return it's port #
    for (uchar i = 0; i < numInPorts; i++)
    {
        QString newPortName = portNameFix(qsFromStd(IN_PORT_MGR->getPortName(i)));

       if (newPortName == thisPortName) return i;
    }
    return -1;
}

int KMI_Ports::getOutPortNumber (QString thisPortName)
{
    if (IN_PORT_MGR == nullptr || OUT_PORT_MGR == nullptr) return -1; // safety check

    //qDebug() << "getOutPortNumber called: " << thisPortName;
    unsigned int numOutPorts = OUT_PORT_MGR->getPortCount();

    // find the matching port name and return it's port #
    for (uchar i = 0; i < numOutPorts; i++)
    {
        QString newPortName = portNameFix(qsFromStd(OUT_PORT_MGR->getPortName(i)));

       if (newPortName == thisPortName) return i;
    }
    return -1;
}

QString KMI_Ports::getInPortName (int thisPort)
{
    if (IN_PORT_MGR == nullptr || OUT_PORT_MGR == nullptr) return ""; // safety check

    QString driverPortName;
    if (thisPort != -1 && (unsigned int)thisPort <= IN_PORT_MGR->getPortCount())
    {
        driverPortName = QString::fromStdString(IN_PORT_MGR->getPortName(thisPort));
    }
    else
    {
        qDebug() << "Couldn't find port: " << thisPort;
        driverPortName = "None";
        return driverPortName;
    }
    return portNameFix(driverPortName);
}

QString KMI_Ports::getOutPortName (int thisPort)
{
    if (IN_PORT_MGR == nullptr || OUT_PORT_MGR == nullptr) return ""; // safety check

    QString driverPortName;
    if (thisPort != -1 && (unsigned int)thisPort <= OUT_PORT_MGR->getPortCount())
    {
        driverPortName = QString::fromStdString(OUT_PORT_MGR->getPortName(thisPort));
    }
    else
    {
        qDebug() << "Couldn't find port: " << thisPort;
        driverPortName = "None";
        return driverPortName;
    }
    return portNameFix(driverPortName);
}

#ifndef Q_OS_WIN
void KMI_Ports::slotCreateVirtualIn(QString portName)
{
    if (IN_PORT_MGR == nullptr || OUT_PORT_MGR == nullptr) return; // safety check

    qDebug() << "slotCreateVirtualIn called";
    try
    {
        IN_PORT_VIRTUAL->openVirtualPort(portName.toStdString());
    }
    catch (RtMidiError &error)
    {
        qDebug() << "openVirtualInPort error:" << (QString::fromStdString(error.getMessage()));
    }
}

void KMI_Ports::slotCreateVirtualOut(QString portName)
{
    if (IN_PORT_MGR == nullptr || OUT_PORT_MGR == nullptr) return; // safety check

    qDebug() << "slotCreateVirtualOut called";
    try
    {
        OUT_PORT_VIRTUAL->openVirtualPort(portName.toStdString());
    }
    catch (RtMidiError &error)
    {
        qDebug() << "openVirtualOutPort error:" << (QString::fromStdString(error.getMessage()));
    }
}

void KMI_Ports::slotCloseVirtualIn()
{
    if (IN_PORT_MGR == nullptr || OUT_PORT_MGR == nullptr) return; // safety check

    qDebug() << "slotCloseVirtualIn called";
    try
    {
        IN_PORT_VIRTUAL->closePort();
    }
    catch (RtMidiError &error)
    {
        qDebug() << "closeVirtualInPort error:" << (QString::fromStdString(error.getMessage()));
    }
}

void KMI_Ports::slotCloseVirtualOut()
{
    if (IN_PORT_MGR == nullptr || OUT_PORT_MGR == nullptr) return; // safety check

    qDebug() << "slotCloseVirtualOut called";
    try
    {
        OUT_PORT_VIRTUAL->closePort();
    }
    catch (RtMidiError &error)
    {
        qDebug() << "closeVirtualOutPort error:" << (QString::fromStdString(error.getMessage()));
    }
}
#endif

void KMI_Ports::listMaps()
{
    qDebug() << "#################################";
    qDebug() << "listMap inputs - count: " << midiInputPorts.count();

    emit signalInputCount(midiInputPorts.count());

    for (QMap<QString, int>::const_iterator it = midiInputPorts.cbegin(), end = midiInputPorts.cend(); it != end; ++it)
    {
        // get the key/value pair
        QString portName = it.key();
        int port = it.value();

        qDebug() << "Name: " << portName << " Port: " << port;
        emit signalInputPort(portName, port);
    }

    qDebug() << "\nlistMap outputs - count: " << midiOutputPorts.count();

    emit signalOutputCount(midiOutputPorts.count());

    for (QMap<QString, int>::const_iterator it = midiOutputPorts.cbegin(), end = midiOutputPorts.cend(); it != end; ++it)
    {
        // get the key/value pair
        QString portName = it.key();
        int port = it.value();

        qDebug() << "Name: " << portName << " Port: " << port;
        emit signalOutputPort(portName, port);
    }
    qDebug() << "#################################\n\n";
}


// If port names are not hard coded then MacOS and Windows modify them, which is annoying but only
// affects QuNexus, SoftStep, and 12Step. New firmware will fix this, but we have to catch legacy issues.
//
//
// TODO: update to handle multiple devices (ie two QuNexus)
QString portNameFix(QString portName)
{
    //qDebug() << "portNameFix called - portName: " << portName;

#ifdef Q_OS_LINUX
    // Linux puts a " 20" at the end of each port name, remove it and this should work
    portName = portName.left(portName.length() -3);
    // Linux puts a "<devicename>:" at the beginning of the portname, remove it
    portName = portName.mid(portName.indexOf(":")+1);
    qDebug() << "Fixed linux portname: " << portName;
#endif


// MacOS reports "[Device] Port 1" etc if ports are not hardcoded, if the OS is not set to english then
// "Port" could be "Portii", "Puerte" etc. This code returns the expected name regardless of language.

    int spacePosition = portName.indexOf(" ");

#ifdef Q_OS_MAC

    QString pnFirstWord;

    if (spacePosition == -1)
    {
        pnFirstWord = portName; // no spaces, return full word
    }
    else
    {
        pnFirstWord = portName.left(spacePosition);
    }

    //qDebug() << "pnFirstWord: " << pnFirstWord;

    // must return the "... Port x" format, helps detect old firmware during update process
    if (pnFirstWord == "QuNexus")
    {
        if (portName.indexOf("1") != -1) // port 1
        {
            //qDebug() << "Fix QuNexus Port 1: " << QUNEXUS_IN_P1;
            return QUNEXUS_OLD_IN_P1; // our input and output ports have the same names
        }
        else if (portName.indexOf("2") != -1) // port 2
        {
            //qDebug() << "Fix QuNexus Port 2: " << QUNEXUS_IN_P2;
            return QUNEXUS_OLD_IN_P2;
        }
        else if (portName.indexOf("3") != -1) // port 3
        {
            //qDebug() << "Fix QuNexus Port 3: " << QUNEXUS_IN_P3;
            return QUNEXUS_OLD_IN_P3;
        }
        else
        {
            return portName; // ports are hardcoded
        }
    }
    else if (pnFirstWord == "SSCOM")
    {
        //qDebug() << "Fix MIDI port name: " << portName;
        if (portName.indexOf("1") != -1) // port 1
        {
            return SS_OLD_IN_P1; // Report the old name so that we know to upgrade to bootloader
        }
        else if (portName.indexOf("2") != -1) // port 2
        {
            return SS_OLD_IN_P2;
        }
    }
    else if (pnFirstWord == "SoftStep")
    {
        //qDebug() << "Fix MIDI port name: " << portName;
        if (portName.indexOf("1") != -1) // port 1
        {
            if (portName.contains("Bootloader"))
            {
                return SS_BL_PORT; // Fix for bad portname translations
            }
            else
            {
                return SS_IN_P1;
            }
        }
        else if (portName.indexOf("2") != -1) // port 2
        {
            if (portName.contains("Bootloader"))
            {
                return portName; // bootloader port 2 does nothing
            }
            else
            {
                return SS_IN_P2;
            }
        }

    }
//    else if (pnFirstWord == "12Step")
//    {
//        //qDebug() << "Fix MIDI port name: " << portName;
//        if (portName.indexOf("1") != -1) // port 1
//        {
//            return TWELVESTEP_IN_P1; // Report the old name so that we know to upgrade to bootloader
//        }
//        else if (portName.indexOf("2") != -1) // port 2
//        {
//            return TWELVESTEP_IN_P2;
//        }
//    }

//    else if (pnFirstWord == "QUNEO") // fix camelcase for QuNeo
//    {
//        return QUNEO_IN_P1;
//    }

    return portName;
#endif

#ifdef Q_OS_WIN

    // Windows is not showing our hardcoded port names, either way it reports:
    // K-Board Pro 4 0
    // MIDIIN2 (K-Board Pro 4) 1

    // remove extra digits
    if (portName.right(2).toInt() > 9 )     // test for two digit port numbers
    {
        portName = portName.left(portName.length() - 3);
    }
    else     // single digit
    {
        portName = portName.left(portName.length() - 2);
    }

    // K-Mix on Windows is a special case, it's driver is tied to the ASIO driver from thesycon.
    // it can show up as "2-Audio Control" or just "Audio Control"
    //qDebug() << "kmix - pre: " << portName << " post: " << portName.mid(2, portName.length() - 2);

    QString portTrimmed = portName.mid(spacePosition + 1, portName.length() - spacePosition + 1);
    // (commented output for QuNeo Branch)
    if (    (portName == KMIX_IN_P1 || portTrimmed == KMIX_IN_P1) ||
            (portName == KMIX_IN_P2 || portTrimmed == KMIX_IN_P2) ||
            (portName == KMIX_IN_P3 || portTrimmed == KMIX_IN_P3) )
    {
        //qDebug() << "K-mix fix - portname: " << portName << " trimmed: " << portTrimmed;
        portName = QString("K-Mix %1").arg(portTrimmed);
        return portName;
    }

    // windows test
    if (portName.indexOf("MIDIIN") == 0)
    {
        int thisMidiDigit = QString(portName.mid(6, 1)).toInt();
        QString retPort;

        // QuNexus
        if (portName.indexOf("QuNexus") != -1)
        {
            if (thisMidiDigit <= lastMIDIIN_QuNexus || lastMIDIIN_QuNexus == 0) // expander port
            {
                retPort = QUNEXUS_IN_P2;
            }
            else if (thisMidiDigit > lastMIDIIN_QuNexus)
            {
                retPort = QUNEXUS_IN_P3;
            }
            //qDebug() << "fixport-- thisMidiDigit: " << thisMidiDigit << " lastMIDIIN_QuNexus: " << lastMIDIIN_QuNexus << " retPort: " << retPort;
            lastMIDIIN_QuNexus = thisMidiDigit;
            return retPort;
        }
        // SoftStep
        else if (portName.indexOf("SSCOM") != -1 || portName.indexOf("SoftStep") != -1)
        {
            return SS_IN_P2; // Two ports means any port with "MIDIIN" is the expander port
        }
    }
    else if (portName.indexOf("MIDIOUT") == 0)
    {
        int thisMidiDigit = QString(portName.mid(7, 1)).toInt();
        QString retPort;

        // QuNexus
        if (portName.indexOf("QuNexus") != -1)
        {
            if (thisMidiDigit <= lastMIDIOUT_QuNexus || lastMIDIOUT_QuNexus == 0) // expander port
            {
                retPort = QUNEXUS_IN_P2;
            }
            else if (thisMidiDigit > lastMIDIOUT_QuNexus)
            {
                retPort = QUNEXUS_IN_P3;
            }
            //qDebug() << "fixport-- thisMidiDigit: " << thisMidiDigit << " lastMIDIOUT_QuNexus: " << lastMIDIOUT_QuNexus << " retPort: " << retPort;
            lastMIDIOUT_QuNexus = thisMidiDigit;
            return retPort;
        }
        // SoftStep
        else if (portName.indexOf("SSCOM") != -1 || portName.indexOf("SoftStep") != -1)
        {
            return SS_OUT_P2; // Two ports means any port with "MIDIIN" is the expander port
        }
    }
    else // first port
    {
        if (portName.indexOf("QuNexus") == 0)
        {
            //qDebug() << "setting to QuNexus Control Surface, called portName: " << portName;
            return QUNEXUS_IN_P1;
        }
        else if (portName.indexOf("QUNEO") == 0) // fix camelcase for QuNeo
        {
            return QUNEO_IN_P1;
        }
        else if (portName.indexOf("SSCOM") == 0)
        {
            return SS_OLD_IN_P1; // Report the old name so that we know to upgrade to bootloader
        }
        else if (portName == "SoftStep")
        {
            return SS_IN_P1;
        }

    }
    return portName; // failsafe, catches control port

#endif

}
