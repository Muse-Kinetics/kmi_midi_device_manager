// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef KMI_FWUPDATE_H
#define KMI_FWUPDATE_H

#include <QByteArray>
#include <QVector>
#include <QString>

class PacketizedFirmwareUpdate
{
public:
    bool initialize(const QByteArray &firmwareBytes, QString *errorMessage);
    void reset();

    bool isLoaded() const;
    bool hasPendingPacket() const;
    const QByteArray &currentPacket() const;

    int currentPacketNumber() const;
    int totalPackets() const;
    int totalBytes() const;
    int sentPackets() const;
    int sentBytes() const;
    int currentPacketSize() const;
    int percentComplete() const;
    int ignoredByteCount() const;

    void markCurrentPacketSent();

private:
    QVector<QByteArray> packets;
    int packetIndex = 0;
    int packetBytesSent = 0;
    int packetBytesTotal = 0;
    int ignoredBytes = 0;
};

#endif // KMI_FWUPDATE_H