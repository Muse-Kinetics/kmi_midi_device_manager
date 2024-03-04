// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "kmiSysEx.h"
#include <string.h>

uint16_t reverseBytes(uint16_t value) {
    return (value >> 8) | (value << 8);
}

kmiSysEx::kmiSysEx()
{

}

KMI_Decode::KMI_Decode()
{

}

void KMI_Decode::slotDecodePacket(QByteArray sysExBA)
{
    uint8_t msgPID;

    uint16_t decodeIndex = 0; // index of the raw encoded data
    uint16_t length = sysExBA.length();; // length of encoded data

    PACKET_PREAMBLE preamble; // buffer for preamble
    PACKET_PAYLOAD payload; // data goes here

    memset(&preamble, 0, sizeof(PACKET_PREAMBLE));
    memset(&payload, 0, sizeof(PACKET_PAYLOAD));

    // verify KMI sysex
    char packetTypeHeader[] = {kmi_id_1, kmi_id_2, kmi_id_3};
    //F0 00 01 5F 19 PID_MSB PID_LSB

    //qDebug() << "sysex id1: " << (uchar)sysExBA[1] << "id2: " << (uchar)sysExBA[2] << "id3: " << (uchar)sysExBA[3];
    if(sysExBA.indexOf(QByteArray(packetTypeHeader, 2)) != 1)
    {
        return; // this is not a kmi packet
    }

    decodeIndex = 5;
    // get PID
    msgPID = sysExBA[decodeIndex++]; // search for decode start after PID
    //qDebug() << "sysex PID: " << msgPID;

    core_sx_packet_init(); // handles CRC and 8bit decode

    while((uchar)sysExBA[decodeIndex++] != 1) // wait for decode start (1)
    {
        if (decodeIndex >= length)
        {
            qDebug("ERROR: decodeIndex > length while waiting for decode start, returning\n");
            return; // error
        }
        if ((uchar)sysExBA[decodeIndex] == MIDI_SX_STOP)
        {
            qDebug("ERROR: unexpected end of sysex, returning\n");
            return; // error
        }
    }

    uint16_t core_sx_count = 0; // index of the decoded data, start at 0

    while(decodeIndex < length)
    {

        val = sysExBA[decodeIndex++];
        //qDebug() << "Input: " << val << " decodeIndex: " << decodeIndex << " length: " << length << "\n";

        midi_sx_decode_put(val); // put 8 bytes into the buffer

        while(midi_sx_decode_get(&val)) // on the 8th byte we decode the previous 7
        {
            // check CRC
            if ( core_sx_count < 4 || // the first 4 bytes are msgCategory, msgType, length msg, length lsb
                ( core_sx_count >= SX_PREAMBLE_SIZE_CRC && // don't CRC bytes 4 and 5 (preamble crc)
                  (payload.index < payload.length) ) )  // or the two bytes after the payload (data crc)
            {
                //int pre_crc = crc;
                crc_byte(val);
                //qDebug() << "crc pre: " << pre_crc <<" post: " << crc << " val: " << val;
            }

            if (core_sx_count < SX_PREAMBLE_SIZE_CRC)
            {
                preamble.raw[core_sx_count++] = val; // store preamble
            }
            else if (payload.index < payload.length)
            {
                qDebug() << "payload[" << payload.index << "/" << payload.length << "]: " << val << " core_sx_count: " << core_sx_count << " crc: " << crc;
                payload.data[payload.index++] = val; // store data
                core_sx_count++; // keep incrementing this
            }
            else if (payload.crc.index == 0)
            {
                payload.crc.MSB = val;
                payload.crc.index++;
                qDebug() << "payload.crc.MSB: " << payload.crc.MSB << " crc: " << crc;
            }
            else if (payload.crc.index == 1)
            {
                payload.crc.LSB = val;
                payload.crc.index++;
                qDebug() << "payload.crc.LSB: " << payload.crc.LSB << " crc: " << crc;
            }
            else
            {
                qDebug() << "ERROR: payload overrun!";

                return;
            }

            if (core_sx_count == 4) // just received length
            {
                preamble.packet.dataLength_with_preamble = reverseBytes(preamble.packet.dataLength_with_preamble); // fix endienness
                payload.length = preamble.packet.dataLength_with_preamble;
                preamble.packet.dataLength_with_preamble += SX_PREAMBLE_SIZE_CRC;
            }

            //qDebug() << "decode - val: " << val << " crc: " << crc << " core_sx_count: " << core_sx_count << " decodeIndex: " << decodeIndex;

            if (core_sx_count == SX_PREAMBLE_SIZE_CRC) // 6 bytes, last two are CRC msb/lsb
            {
                preamble.packet.crc = reverseBytes(preamble.packet.crc); // fix endienness

                qDebug() <<	"msgCategory: " << preamble.packet.category <<
                        " msgType: " << preamble.packet.type <<
                        " payload.lngth: " << payload.length <<
                        " dataLength_with_preamble: " << preamble.packet.dataLength_with_preamble <<
                        " preambleCRC: " << preamble.packet.crc <<
                        "\n";
                if (crc != preamble.packet.crc)
                {
                    qDebug("ERROR: preambleCRC fail!\n");
                    return;
                }
                qDebug("preambleCRC pass!\n");
                crc_init();

            }
            else if (payload.crc.index) // look for end of payload
            {
                // payload finished

                if (payload.crc.index == 2) // last message
                {
                    payload.crc.index++;
                    //payload.crc.LSB = val;
                    payload.crc.whole = get16bit(payload.crc.MSB, payload.crc.LSB);
                    qDebug() << "crc.index: " << payload.crc.index << " payload.CRC: " << payload.crc.whole << " payload.crc.MSB: " << payload.crc.MSB << " payload.crc.LSB: " << payload.crc.LSB << " crc: " << crc << "\n";
                    if (payload.crc.whole == crc)
                    {
                        qDebug("Payload CRC pass, emitting message signal!\n");
                        sx_decode_init(); // stop decoding
                        emit signalRxKMIPacket(msgPID, preamble.packet.category, preamble.packet.type, &payload.data[0], payload.length);
                        return;
                    }
                    else
                    {
                        qDebug("ERROR: Payload CRC Fail!\n");
                        sx_decode_init(); // stop decoding
                        return;
                    }
                }
                else if (payload.crc.index > 2)
                {
                    payload.crc.index++;
                    qDebug() << "ERROR: byte rx after payload - crc.Index: " << payload.crc.index << " val: " << val << " core_sx_count: " << core_sx_count << " decodeIndex: " << decodeIndex << "\n";;
                }
            }
        }
        //qDebug() << "finished midi_sx_decode_get, decodeIndex: " << decodeIndex << " length: " << length;
    }
}


// **************************************
// functions below decode 7bits to 8bits
// **************************************

uint16_t KMI_Decode::get16bit(uint8_t msb, uint8_t lsb)
{
    return ((msb << 8) | lsb) & 0xFFFF; // Ensure the result is a 16-bit integer
}

void KMI_Decode::core_sx_packet_init(void) {
    sx_decode_init();
    crc_init();
}

void KMI_Decode::sx_decode_init(void) {
    core_sx_decode.index_in = core_sx_decode.index_out = 0;
}

void KMI_Decode::midi_sx_decode_put(unsigned char val)
{
    core_sx_decode.buf[core_sx_decode.index_in++] = val;
}

uint8_t KMI_Decode::midi_sx_decode_get(unsigned char *val)
{
    if (core_sx_decode.index_in==SX_ENCODE_LEN+1)
    {
        *val = core_sx_decode.buf[core_sx_decode.index_out++];
        if (core_sx_decode.buf[SX_ENCODE_LEN] & 1)
            *val |= 0x80;
        core_sx_decode.buf[SX_ENCODE_LEN] >>=1;
        if (core_sx_decode.index_out==SX_ENCODE_LEN)
        {
            sx_decode_init();
        }
        return 1;
    }
    return 0;
}

void KMI_Decode::crc_init(void) {
    crc = 0xffff;
}

void KMI_Decode::crc_byte(char val)
{
    uint16_t temp;
    uint16_t quick;

    temp = (crc >> 8) ^ val;
    crc <<= 8;
    quick = temp ^ (temp >> 4);
    crc ^= quick;
    quick <<= 5;
    crc ^= quick;
    quick <<= 7;
    crc ^= quick;
}

//void SysexData::crc_byte(char val)   {
//        unsigned short temp;
//        unsigned short quick;

//        temp = (crc >> 8) ^ val;
//        crc <<= 8;
//        quick = temp ^ (temp >> 4);
//        crc ^= quick;
//        quick <<= 5;
//        crc ^= quick;
//        quick <<= 7;
//        crc ^= quick;
//}

// **************************************
// functions below encode 8bits to 7 bits
// **************************************

KMI_Encode::KMI_Encode(uint8_t _PID)
{
    PID = _PID;
}

void KMI_Encode::midi_buffer_put_nulls(int count)
{
    while(count--)
        msg[msgIndex++] = (0);
}

void KMI_Encode::midi_sx_header()
{
    msgIndex = 0;
    msg[msgIndex++] = (MIDI_SX_START);
    msg[msgIndex++] = (kmi_id_1);
    msg[msgIndex++] = (kmi_id_2);
    msg[msgIndex++] = (kmi_id_3);
    msg[msgIndex++] = (0); // MIDI PID MSB
    msg[msgIndex++] = (PID); // MIDI PID LSB
    msg[msgIndex++] = (0x00); // format
    midi_buffer_put_nulls(4);
}

void KMI_Encode::midi_sx_data_crc(void *data,unsigned short length)
{
    int i;

    for(i=0;i<length;i++)
    {
        midi_sx_encode_crc_char( ((unsigned char *) data)[i]);
    }
}

void KMI_Encode::midi_sx_packet_data(void *source,unsigned short length)
{
    crc = 0xffff; // init crc
    midi_sx_data_crc(source,length);
}

// if we are sending multiple data packets, then we encode the length of the next packet
void KMI_Encode::midi_sx_packet_data_close(unsigned short length)
{
    midi_sx_encode_crc_int(length ? length + TAIL_LEN : 0); // no more packets then encode 0
    midi_sx_encode_int(crc);
}

void KMI_Encode::midi_sx_packet_preamble(unsigned short packet_type,unsigned short length)
{
    crc = 0xffff; // init crc

    msg[msgIndex++] = (0x01); // indicate we are about to begin encoding
    midi_chunk_init(); // being 8bit->7bit encoding
    midi_sx_encode_crc_int(packet_type); // 12step2 this became two chars for category/type
    midi_sx_encode_crc_int(length + TAIL_LEN);
    midi_sx_encode_int(crc);

}

void KMI_Encode::slotEncodePacket(uint8_t category, uint8_t type, uint8_t *ptr, uint16_t length)
{
    qDebug() << "slotEncodePacket called - category: " << category << " type: " << type << " length: " << length;
    uint16_t packet_type = ((category << 8) & 0xFF00) | type;

    midi_sx_header();
    midi_sx_packet_preamble(packet_type, length);
    if (length)
    {
        midi_sx_packet_data(ptr, length);
        midi_sx_packet_data_close(0);    // length 0 to indicate no more data packets
    }
    midi_sx_flush(); // finish encoding frame
    midi_sx_close(); // end syx

    emit signalSendSysEx(&msg[0], msgIndex-1);
    msgIndex = 0;
}

void KMI_Encode::midi_sx_close(void) {
    msg[msgIndex++] = (MIDI_SX_STOP);
}

void KMI_Encode::midi_chunk_init(void)
{
    midi_hi_bits = midi_hi_count = 0;
}

void KMI_Encode::midi_sx_encode_char(unsigned char val)
{
    midi_hi_bits |= (val & 0x80);
    midi_hi_bits >>= 1;
    msg[msgIndex++] = (val & 0x7f);
    if (++midi_hi_count == SX_ENCODE_LEN)
    {
        midi_hi_count = 0;
        msg[msgIndex++] = (midi_hi_bits);
    }
}

void KMI_Encode::midi_sx_encode_crc_char(unsigned char val)
{
    crc_byte(val);
    midi_sx_encode_char(val);
}

void KMI_Encode::midi_sx_encode_crc_int(unsigned int val)
{
    midi_sx_encode_crc_char(val>>8);
    midi_sx_encode_crc_char(val);
}

void KMI_Encode::midi_sx_encode_int(unsigned int val)
{
    midi_sx_encode_char(val>>8);
    midi_sx_encode_char(val);
}

void KMI_Encode::midi_sx_flush(void)
{
    while(midi_hi_count)
        midi_sx_encode_char(0);
}

void KMI_Encode::crc_init(void) {
    crc = 0xffff;
}

void KMI_Encode::crc_byte(char val)
{
    uint16_t temp;
    uint16_t quick;

    temp = (crc >> 8) ^ val;
    crc <<= 8;
    quick = temp ^ (temp >> 4);
    crc ^= quick;
    quick <<= 5;
    crc ^= quick;
    quick <<= 7;
    crc ^= quick;
}
