// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef CVCALDATA_H
#define CVCALDATA_H

#include <cstdint>

// some CS 101 notation required here to keep me sane, EB
typedef union {
    uint16_t value;
    struct {
        uint8_t lsb; // address of value
        uint8_t msb; // address of value +1
    } byte;
} LittleEndianUint16; // "endian" refers to the rightmost byte

typedef union {
    uint16_t value;
    struct {
        uint8_t msb; // address of value
        uint8_t lsb; // address of value +1
    } byte;
} BigEndianUint16;



class CVCalData
{

public:

    static constexpr uint8_t NumHeaderBytes = 2;
    static constexpr uint8_t NumCVOuts = 2;
    static constexpr uint8_t NumCVOctaves = 6;
    static constexpr uint8_t NumCVNotes = 61;
    static constexpr uint16_t arraySize = NumHeaderBytes + (NumCVOuts * (NumCVOctaves + NumCVNotes) * 2);

    typedef struct {
        int8_t version;
        uint8_t cal_mode;
        LittleEndianUint16 octaves[NumCVOuts][NumCVOctaves];
        LittleEndianUint16 notes[NumCVOuts][NumCVNotes];
    } CAL_LE;
    typedef struct {
        int8_t version;
        uint8_t cal_mode;
        BigEndianUint16 octaves[NumCVOuts][NumCVOctaves];
        BigEndianUint16 notes[NumCVOuts][NumCVNotes];
    } CAL_BE;

    union
    {
        uint8_t raw[arraySize];
        CAL_LE LE;
        CAL_BE BE;
        struct {
            int8_t version;
            uint8_t cal_mode;
            uint16_t octaves[NumCVOuts][NumCVOctaves];
            uint16_t notes[NumCVOuts][NumCVNotes];
        };

    } data;


    uint8_t *data_bytestreamMSBthenLSB[arraySize];
    uint8_t *data_bytestreamLSBthenMSB[arraySize];

    explicit CVCalData()
    {
        // initialize header
        data.version = -1; // version not set
        data.cal_mode = 0; // 0 = CV_CAL_MODE_FACTORY

        // setup pointers
        int iMtL = 0;
        int iLtM = 0;

        // octaves
        for (uint8_t cv = 0; cv < NumCVOuts; cv++)
        {
            for (uint8_t octave = 0; octave < NumCVOctaves; octave++)
            {
                data_bytestreamMSBthenLSB[iMtL++] = &data.LE.octaves[cv][octave].byte.msb; // the byte stream rx has the msb first
                data_bytestreamMSBthenLSB[iMtL++] = &data.LE.octaves[cv][octave].byte.lsb;

                data_bytestreamLSBthenMSB[iLtM++] = &data.BE.octaves[cv][octave].byte.lsb;
                data_bytestreamLSBthenMSB[iLtM++] = &data.BE.octaves[cv][octave].byte.msb;
            }
        }

        // notes
        for (uint8_t cv = 0; cv < NumCVOuts; cv++)
        {
            for (uint8_t note = 0; note < NumCVNotes; note++)
            {
                data_bytestreamMSBthenLSB[iMtL++] = &data.LE.notes[cv][note].byte.msb;
                data_bytestreamMSBthenLSB[iMtL++] = &data.LE.notes[cv][note].byte.lsb;

                data_bytestreamLSBthenMSB[iLtM++] = &data.BE.notes[cv][note].byte.lsb;
                data_bytestreamLSBthenMSB[iLtM++] = &data.BE.notes[cv][note].byte.msb;
            }
        }
    }

    bool systemIsLittleEndian() const {
            static const uint16_t testVal = 1; // Use static to initialize once
            return reinterpret_cast<const uint8_t*>(&testVal)[0] == 1;
    }
};

#endif // CVCALDATA_H
