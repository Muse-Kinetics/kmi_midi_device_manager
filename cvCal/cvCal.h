// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef CVCAL_H
#define CVCAL_H

#include <QWidget>
#include <QDialog>

#include <QTableWidget>
#include <QLineEdit>
#include <QIntValidator>
#include <QFile>
#include <QSpinBox>
#include <QKeyEvent>
#include <QLineEdit>
#include <QFile>
#include <QSettings>

#include <sysexcmds.h>
#include <cvCalData.h>
#include <kmiSpinBoxUpDown.h>
#include <midi.h> // max midi channels
#include <sysex.h> // packet types

#define CURRENT_CV_CAL_VERSION 1

enum
{
    CV_OUT1,
    CV_OUT2
};

typedef struct {
    uint8_t data[278];
} CV_CALIBRATION_RAW;

namespace Ui {
class cvCal;
}

class cvCal : public QDialog
{
    Q_OBJECT

public:
    explicit cvCal(QWidget *parent = nullptr);
    ~cvCal();

    CVCalData cvCalData;

    uint16_t get16bit(uint8_t msb, uint8_t lsb);
    uint16_t reverseBytes(uint16_t value);

    uint8_t nrpnChannel;

    QSettings *sessionSettings;

    const QMap<QString, unsigned char> calModeMap =
    {
        {"Factory", 0},
        {"Octaves", 1},
        {"Notes", 2}
    };

    QFile *cvStyleFile;
    QString cvStyleString;

signals:
    void signalWindowClosed();
    void signalSendStepSXPacket(uint8_t category, uint8_t type, uint8_t *ptr, uint16_t length);
    void signalSendNRPN(int parameter, int value, uchar channel);

public slots:
    void slotConnectElements();
    void slotDisconnectElements();

    void slotUpdateNRPNChannel(int channel);

    void slotValueChanged();
    void slotCalcNotes();

    void slotUpdateUiVals();
    void slotParseDeviceCVCalibration(uint8_t *ptr, uint16_t length);

    void slotGetDeviceCVCalibration();
    void slotResetDeviceCVCalibration();

    void slotUpdateCVcalData();
    void slotSendCalibrationData();


private:
    Ui::cvCal *ui;

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
};

#endif // CVCAL_H
