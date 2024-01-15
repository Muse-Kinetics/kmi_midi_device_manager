// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "pedalcal.h"
#include "ui_pedalcal.h"

const unsigned char table_Logarithmic[] = {0,3,7,10,13,16,19,22,24,27,29,32,34,36,38,40,42,43,45,47,49,50,52,53,55,56,58,59,60,62,63,64,65,67,68,69,70,71,72,73,74,75,76,77,78,79,81,81,82,83,84,85,85,86,87,88,89,89,90,91,92,92,93,94,95,95,96,97,97,98,99,99,100,101,101,102,103,103,104,104,105,106,106,107,107,108,108,109,110,110,111,111,112,112,113,113,114,114,115,115,116,116,117,117,118,118,119,119,119,120,120,121,121,122,122,123,123,123,124,124,125,125,125,126,126,127,127,127};
const unsigned char table_Sin[] = {0,0,0,0,0,0,0,0,1,1,1,2,2,3,3,4,4,5,6,6,7,8,9,10,10,11,12,13,14,15,16,17,19,20,21,22,23,24,26,27,28,30,31,32,34,35,37,38,40,41,43,44,46,47,49,50,52,53,55,56,58,60,61,63,64,66,67,69,71,72,74,75,77,78,80,81,83,84,86,87,89,90,92,93,95,96,97,99,100,101,103,104,105,106,107,108,110,111,112,113,114,115,116,117,117,118,119,120,121,121,122,123,123,124,124,125,125,126,126,126,127,127,127,127,127,127,127,127};
const unsigned char table_Cos[] = {0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,3,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,11,11,12,13,13,14,15,16,16,17,18,19,20,21,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,38,39,40,41,42,43,45,46,47,48,49,51,52,53,55,56,57,59,60,61,63,64,65,67,68,70,71,72,74,75,77,78,80,81,83,84,86,87,89,90,92,93,95,96,98,99,101,102,104,105,107,109,110,112,113,115,116,118,120,121,123,124,126,127};
const unsigned char table_Exponential[] = {0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,4,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,12,12,13,13,14,15,16,16,17,18,19,19,20,21,22,23,24,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,40,41,42,43,44,45,47,48,49,50,52,53,54,55,57,58,60,61,62,64,65,67,68,70,71,73,74,76,77,79,80,82,84,85,87,89,90,92,94,96,97,99,101,103,104,106,108,110,112,114,116,118,120,122,124,125,127};
const unsigned char table_deadzone[] = {0,1,2,3,4,5,7,8,9,10,11,13,14,15,16,17,19,20,21,22,23,24,26,27,28,29,30,32,33,34,35,36,38,39,40,41,42,43,45,46,47,48,49,51,52,53,54,55,57,58,59,60,61,62,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,64,65,66,67,68,70,71,72,73,74,76,77,78,79,80,81,83,84,85,86,87,89,90,91,92,93,95,96,97,98,99,100,102,103,104,105,106,108,109,110,111,112,113,115,116,117,118,119,121,122,123,124,125,127};

const unsigned char *tablePtr[] =
{
    table_Logarithmic, // dummy placeholder for linear
    table_Sin,
    table_Cos,
    table_Exponential,
    table_Logarithmic,
    table_deadzone
};

pedalCal::pedalCal(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::pedalCal)
{
    ui->setupUi(this);
    slotConnectElements();
    slotSetDefaultValues();
}

pedalCal::~pedalCal()
{
    slotDisconnectElements();
    delete ui;
}

void pedalCal::closeEvent(QCloseEvent *event)
{
    qDebug() << "pedalCal closeEvent";
    emit signalWindowClosed();
    QWidget::closeEvent(event);
}

void pedalCal::slotConnectElements()
{
    // sliders
    connect(ui->slider_in, SIGNAL(valueChanged(int)), this, SLOT(slotSetInput(int)));
    connect(ui->slider_min, SIGNAL(valueChanged(int)), this, SLOT(slotSetMin(int)));
    connect(ui->slider_max, SIGNAL(valueChanged(int)), this, SLOT(slotSetMax(int)));

    // butons
    connect(ui->button_restore, SIGNAL(clicked()), this, SLOT(slotSetDefaultValues()));
    connect(ui->button_cancel, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->button_save, SIGNAL(clicked()), this, SLOT(slotSaveAndSendCalibrationValues()));

}

void pedalCal::slotDisconnectElements()
{
    disconnect(ui->slider_in, SIGNAL(valueChanged(int)), this, SLOT(slotSetInput(int)));
    disconnect(ui->slider_min, SIGNAL(valueChanged(int)), this, SLOT(slotSetMin(int)));
    disconnect(ui->slider_max, SIGNAL(valueChanged(int)), this, SLOT(slotSetMax(int)));
}

// *****************************************************

void pedalCal::slotSetDefaultValues()
{
    calMin = 50;
    calMax = 233;
    ui->dropdown_table->setCurrentIndex(0);
    ui->slider_min->setValue(calMin);
    ui->slider_max->setValue(calMax);
}

// this functinon handles incomming tether data
void pedalCal::slotProcessInput(int val)
{
    qDebug() << "slotProcessInput called - val: " << val;
    ui->slider_in->setValue(val);
    slotSetInput(val);
}

// this function also handles changes from the ui slider during debugging
void pedalCal::slotSetInput(int val)
{
    qDebug() << "slotSetInput called - val: " << val;
    inputVal = val;
    ui->label_in_val->setText(QString::number(val));
    slotCalculateOutput();
}

void pedalCal::slotSetMin(int val)
{
    qDebug() << "slotSetMin called - val: " << val;
    calMin = val;
    ui->label_min_val->setText(QString::number(val));
    slotCalculateOutput();
}

void pedalCal::slotSetMax(int val)
{
    qDebug() << "slotSetMax called - val: " << val;
    calMax = val;
    ui->label_max_val->setText(QString::number(val));
    slotCalculateOutput();
}

//*************************************************
//*** remap_charToChar(val, in_min, in_max, out_min, out_max)
//*************************************************
unsigned char remap_charToChar(unsigned char val, unsigned char in_min, unsigned char in_max, unsigned char out_min, unsigned char out_max)
{
    unsigned char temp = 0;

    if(val <= in_min)
    {
        return out_min;
    }
    else if(val >= in_max)
    {
        return out_max;
    }

    temp = ((unsigned int)(val - in_min) * (out_max - out_min) / (in_max - in_min)) + out_min;

    return temp;
}

void pedalCal::slotCalculateOutput()
{
    unsigned char tableIndex = ui->dropdown_table->currentIndex();

    outputVal = remap_charToChar(inputVal, calMin, calMax, 0, 127);

    if (tableIndex) // ignores index 0, linear
    {
        qDebug() << "table index: " << tableIndex;
        outputVal = tablePtr[tableIndex][outputVal];
    }

    ui->slider_out->setValue(outputVal);
    ui->label_out_val->setText(QString::number(outputVal));
    qDebug() << "slotCalculateOutput called - outputVal: " << outputVal;
}

/*
    The factory settings json file does not contain pedal calibration values. The first
    time a user saves/sends calibtation data, we write it to settings.json and thereafter
    we will load that data when the calibration window is loaded.
*/
void pedalCal::slotLoadJSONCalibrationValues(QVariantMap settings, QVariantMap)
{
    qDebug() << "pedalCal slotLoadJSONCalibrationValues called - pedal_calibration_min: " << settings.value("pedal_calibration_min").toInt();
    if (settings.value("pedal_calibration_min").isNull() == false)
    {
        calMin = settings.value("pedal_calibration_min").toInt();
        ui->slider_min->setValue(calMin);
    }
    if (settings.value("pedal_calibration_max").isNull() == false)
    {
        calMax = settings.value("pedal_calibration_max").toInt();
        ui->slider_max->setValue(calMax);
    }
    if (settings.value("pedal_calibration_table").isNull() == false)
    {
        ui->dropdown_table->setCurrentIndex(settings.value("pedal_calibration_table").toUInt());
    }
}

void pedalCal::slotSaveAndSendCalibrationValues()
{
    unsigned char tableIndex = ui->dropdown_table->currentIndex();

    qDebug() << "slotSendCalibrationValues called - calMin: " << calMin << " calMax:" << calMax << " tableIndex: " << tableIndex;

    emit signalStoreValue("pedal_calibration_min", calMin);
    emit signalStoreValue("pedal_calibration_max", calMax);
    emit signalStoreValue("pedal_calibration_table", tableIndex);

    emit signalSendCalibration();
    emit signalSaveCalibration();

    this->close();
}

