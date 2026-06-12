// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "KMI_fwupdate.h"

#include <QtMath>

bool PacketizedFirmwareUpdate::initialize(const QByteArray &firmwareBytes, QString *errorMessage)
{
    reset();

    int packetStart = -1;

    for (int index = 0; index < firmwareBytes.size(); ++index)
    {
        const unsigned char currentByte = static_cast<unsigned char>(firmwareBytes.at(index));

        if (currentByte == 0xF0)
        {
            if (packetStart >= 0)
            {
                if (errorMessage != nullptr)
                {
                    *errorMessage = "Malformed firmware file: nested SysEx start byte.";
                }
                reset();
                return false;
            }

            packetStart = index;
            continue;
        }

        if (currentByte == 0xF7)
        {
            if (packetStart < 0)
            {
                ignoredBytes++;
                continue;
            }

            const QByteArray packet = firmwareBytes.mid(packetStart, index - packetStart + 1);
            packets.append(packet);
            packetBytesTotal += packet.size();
            packetStart = -1;
            continue;
        }

        if (packetStart < 0)
        {
            ignoredBytes++;
        }
    }

    if (packetStart >= 0)
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = "Malformed firmware file: unterminated SysEx packet.";
        }
        reset();
        return false;
    }

    if (packets.isEmpty())
    {
        if (errorMessage != nullptr)
        {
            *errorMessage = "Malformed firmware file: no complete SysEx packets found.";
        }
        reset();
        return false;
    }

    return true;
}

void PacketizedFirmwareUpdate::reset()
{
    packets.clear();
    packetIndex = 0;
    packetBytesSent = 0;
    packetBytesTotal = 0;
    ignoredBytes = 0;
}

bool PacketizedFirmwareUpdate::isLoaded() const
{
    return !packets.isEmpty();
}

bool PacketizedFirmwareUpdate::hasPendingPacket() const
{
    return packetIndex < packets.size();
}

const QByteArray &PacketizedFirmwareUpdate::currentPacket() const
{
    return packets[packetIndex];
}

int PacketizedFirmwareUpdate::currentPacketNumber() const
{
    return packetIndex + 1;
}

int PacketizedFirmwareUpdate::totalPackets() const
{
    return packets.size();
}

int PacketizedFirmwareUpdate::totalBytes() const
{
    return packetBytesTotal;
}

int PacketizedFirmwareUpdate::sentPackets() const
{
    return packetIndex;
}

int PacketizedFirmwareUpdate::sentBytes() const
{
    return packetBytesSent;
}

int PacketizedFirmwareUpdate::currentPacketSize() const
{
    if (!hasPendingPacket())
    {
        return 0;
    }

    return packets[packetIndex].size();
}

int PacketizedFirmwareUpdate::percentComplete() const
{
    if (packetBytesTotal <= 0)
    {
        return 0;
    }

    return qBound(0, qRound((static_cast<double>(packetBytesSent) * 100.0) / static_cast<double>(packetBytesTotal)), 100);
}

int PacketizedFirmwareUpdate::ignoredByteCount() const
{
    return ignoredBytes;
}

void PacketizedFirmwareUpdate::markCurrentPacketSent()
{
    if (!hasPendingPacket())
    {
        return;
    }

    packetBytesSent += packets[packetIndex].size();
    packetIndex++;
}