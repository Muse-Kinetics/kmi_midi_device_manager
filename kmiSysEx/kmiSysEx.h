// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef KMISYSEX_H
#define KMISYSEX_H

#include <QByteArray>
#include <KMI_mdm.h>
#include <midi.h>
#include <12step.h>

// constants

#define SX_PREAMBLE_SIZE 4
#define SX_PREAMBLE_SIZE_CRC 6
#define SX_PREAMBLE_SIZE_COMPLETE 7
#define SX_ENCODE_LEN 7
#define MAX_SX_BUFFER_SIZE 1024
#define	TAIL_LEN	4 // length[msb/lsb], crc[msb/lsb]


typedef union {
    unsigned char raw[6];
    struct
    {
        uint8_t category;
        uint8_t type;
        uint16_t dataLength_with_preamble, crc;
    } packet;
} PACK_INLINE PACKET_PREAMBLE;


typedef struct
{
    uint16_t index;
    uint16_t length;
    struct {
        uint8_t LSB;
        uint8_t MSB;
        uint16_t index;
//        uint16_t indexMSB; // location in the data payload of the CRC_MSB bye
//        uint16_t indexLSB; // ... LSB
        uint16_t whole;
    } crc;
    uint8_t data[MAX_SX_BUFFER_SIZE];
} PACK_INLINE PACKET_PAYLOAD;



class KMI_Decode : public QWidget
{
    Q_OBJECT
public:
    KMI_Decode();

    // decode 7bit->8bit
    uint16_t get16bit(uint8_t msb, uint8_t lsb);
    void core_sx_packet_init(void);

    void sx_decode_init(void);
    void midi_sx_decode_put(unsigned char val);
    uint8_t midi_sx_decode_get(unsigned char *val);

    void crc_init(void);
    void crc_byte(char val);

signals:
    void signalRxKMIPacket(uint8_t PID, uint8_t category, uint8_t type, uint8_t *ptr, uint16_t length);


public slots:
    void slotDecodePacket(QByteArray sysExBA);


private:


    uint16_t crc = 0;

    struct
    {
        unsigned char 	index_in,
                        index_out,
                        buf[SX_ENCODE_LEN+1];
    } core_sx_decode;

    uint8_t val = 0; // need to rename this


    uint8_t msg[MAX_SX_BUFFER_SIZE];


};


class KMI_Encode : public QWidget
{
    Q_OBJECT
public:
    KMI_Encode(uint8_t _PID);

    uint8_t PID;

    void midi_buffer_put_nulls(int count);
    void midi_sx_header(void);
    void midi_sx_close(void);
    void midi_sx_packet_preamble(uint16_t packet_type, uint16_t length);
    long midi_sx_length(void);
    long midi_sx_byte(int index);
    void midi_sx_data_crc(void *data, uint16_t length);

    void midi_sx_packet_data(void *source, uint16_t length);
    void midi_sx_packet_data_close(uint16_t length);

    void midi_chunk_init(void);
    void midi_sx_encode_int(unsigned int val);
    void midi_sx_flush(void);
    void midi_sx_encode_crc_int(unsigned int val);
    void midi_sx_encode_crc_char(unsigned char val);
    void midi_sx_encode_char(unsigned char val);

    // crc
    void crc_init(void);
    void crc_byte(char val);

signals:
    void signalSendSysExBA(QByteArray);
    void signalSendSysEx(unsigned char *sysEx, int len);


public slots:
    void slotEncodePacket(uint8_t category, uint8_t type, uint8_t *ptr, uint16_t length); // replaces midi_sx_packet

private:

     uint8_t msg[MAX_SX_BUFFER_SIZE];
     uint16_t msgIndex = 0;

     unsigned char midi_hi_bits, midi_hi_count;
     unsigned int midi_packet_num_tx;

     uint16_t crc = 0;

};

class kmiSysEx : public QWidget
{
    Q_OBJECT
public:
    kmiSysEx();


};


#endif // KMISYSEX_H
