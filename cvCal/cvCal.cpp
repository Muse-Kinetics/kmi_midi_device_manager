// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "cvCal.h"
#include "ui_cvCal.h"



cvCal::cvCal(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::cvCal)
{

    sessionSettings = new QSettings(this);

    ui->setupUi(this);
    this->setWindowTitle("CV Calibration");
    this->setFixedSize(800,370);

    this->installEventFilter(this);
    ui->cv_cal_mode->installEventFilter(this);

    slotConnectElements();
    ui->no_focus->setFocus();

    nrpnChannel = MIDI_CH_10; // default nrpn channel for editor and firmware

    // styles etc

    QPalette palette;

    qDebug() << "set cvCal palette";

    // Set palette colors based on your stylesheet
    palette.setColor(QPalette::Window, QColor(10, 10, 10, 90)); // For QWidget background
    palette.setColor(QPalette::ButtonText, Qt::white); // For QPushButton text color
    palette.setColor(QPalette::Button, QColor(60, 60, 60)); // For QPushButton background
    palette.setColor(QPalette::WindowText, QColor(242, 242, 242)); // For QLabel text color

    this->setPalette(palette);

#ifndef Q_OS_MAC
    QString textEditHTML = R"(
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0//EN" "http://www.w3.org/TR/REC-html40/strict.dtd">
    <html>
    <body style="font-family:'Open Sans'; font-size:9pt; font-weight:400; font-style:normal;">Each 12bit value (0-4095) is a scaling calibration value for the listed voltage. <br /><br />
 To calibrate a voltage value, measure the CV out with a precision voltmeter, or connect it to a trusted oscillator and measure the pitches with a tuner. When you adjust a value, the CV is immediately updated so that it can be measured in real time. <br /><br />
You can directly control the CVs with 12 bit values by using NRPN 1 for CV1, and NRPN 2 for CV2. <br /><br />
    </body>
    </html>
    )";


    cvStyleFile = new QFile(":/stylesheets/cvCalStyleWin.qss");
#else

    QString textEditHTML = R"(
<!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0//EN" "http://www.w3.org/TR/REC-html40/strict.dtd">
    <html>
    <body style="font-family:'Open Sans'; font-size:12pt; font-weight:400; font-style:normal;">Each 12bit value (0-4095) is a scaling calibration value for the listed voltage. <br /><br />
 To calibrate a voltage value, measure the CV out with a precision voltmeter, or connect it to a trusted oscillator and measure the pitches with a tuner. When you adjust a value, the CV is immediately updated so that it can be measured in real time. <br /><br />
You can directly control the CVs with 12 bit values by using NRPN 1 for CV1, and NRPN 2 for CV2. <br /><br />
    </body>
    </html>
    )";

    cvStyleFile = new QFile(":/stylesheets/cvCalStyleMac.qss");
#endif

    ui->text_instructions->setHtml(textEditHTML);

    if (!cvStyleFile->open(QFile::ReadOnly))
    {
        qDebug() << "Error opening cvCal stylesheet";
    }
    cvStyleString = QLatin1String(cvStyleFile->readAll());



    qDebug() << "set cvCal stylesheet";
    this->setStyleSheet(cvStyleString);

}

cvCal::~cvCal()
{
    slotDisconnectElements();
    delete ui;
    qDebug() << "cvCal ui deleted";
}

void cvCal::closeEvent(QCloseEvent *event)
{
    qDebug() << "cvCal closeEvent";
    emit signalWindowClosed();
    QWidget::closeEvent(event);
}

bool cvCal::eventFilter(QObject *obj, QEvent *event)
{
    kmiSpinBoxUpDown *spinbox = qobject_cast<kmiSpinBoxUpDown *>(obj);
    QComboBox *comboBox = qobject_cast<QComboBox *>(obj);

    bool toolTipsEnabled = sessionSettings->value("toolTipsEnabled").toBool();

    if(event->type() == QEvent::ToolTip)
    {
        if (!toolTipsEnabled)
        {
            //qDebug() << "suppresed";
            return true;
        }
        else
        {
            //qDebug() << "allowed";
            return QObject::eventFilter(obj, event);
        }
    }

    if (spinbox && event->type() == QEvent::FocusOut && spinbox->text().isEmpty())
    {
        spinbox->setValue(0); // Or your logic to set to a default/previous value
        qDebug() << "Focus out" << spinbox->objectName();
        return true; // Indicate the event has been handled
    }
    else if (event->type() == QEvent::KeyPress)
    {
        qDebug() << "keyPress";
        QKeyEvent *keyEvent = (QKeyEvent*)event;

        switch (keyEvent->key())
        {
            case Qt::Key_Return:
            case Qt::Key_Enter:
            {
                qDebug() << "clear focus";
                ui->no_focus->setFocus();
                return true; // Handled
            }
            case Qt::Key_Escape:
            {
                if (spinbox)
                {
                    qDebug() << "clear focus";
                    ui->no_focus->setFocus();
                    return true; // Handled
                }
                break;
            }
        }
    }
    else if (event->type() == QEvent::MouseButtonPress)
    {
        // Cast the QObject to QWidget to access the `inherits` method
        QWidget *widget = qobject_cast<QWidget *>(obj);
        if (widget && !comboBox)
        {
            // Check if the widget is not a QSpinBox or QPushButton
            if (!widget->inherits("QSpinBox") && !widget->inherits("QPushButton"))
            {

                qDebug() << "clear focus - object: " << widget->objectName() << " meta: " << obj->metaObject()->className();
                ui->no_focus->setFocus();

                return true; // Return true if you want to stop further processing of the event
            }
        }
    }

    return QObject::eventFilter(obj,event); // still process the eevent
}

void cvCal::slotConnectElements()
{
    // spinboxes
    foreach (kmiSpinBoxUpDown *spinbox, this->findChildren<kmiSpinBoxUpDown *>())
    {
        connect(spinbox, SIGNAL(valueChanged(int)), this, SLOT(slotValueChanged()));
    }

    connect(ui->cv_cal_mode, SIGNAL(currentIndexChanged(int)), this, SLOT(slotValueChanged()));

    // butons
    connect(ui->button_calcNotes, SIGNAL(clicked()), this, SLOT(slotCalcNotes()));
    connect(ui->button_restore, SIGNAL(clicked()), this, SLOT(slotResetDeviceCVCalibration()));
    connect(ui->button_cancel, SIGNAL(clicked()), this, SLOT(close()));
    connect(ui->button_save, SIGNAL(clicked()), this, SLOT(slotSendCalibrationData()));

}

void cvCal::slotDisconnectElements()
{
    qDebug() << "cvCal slotDisconnectElements called";
    foreach (kmiSpinBoxUpDown *spinbox, this->findChildren<kmiSpinBoxUpDown *>())
    {
        disconnect(spinbox, SIGNAL(valueChanged(int)), this, SLOT(slotValueChanged()));
    }

    disconnect(ui->cv_cal_mode, SIGNAL(currentIndexChanged(int)), this, SLOT(slotValueChanged()));

    disconnect(ui->button_calcNotes, SIGNAL(clicked()), this, SLOT(slotCalcNotes()));
    disconnect(ui->button_restore, SIGNAL(clicked()), this, SLOT(slotResetDeviceCVCalibration()));
    disconnect(ui->button_cancel, SIGNAL(clicked()), this, SLOT(close()));
    disconnect(ui->button_save, SIGNAL(clicked()), this, SLOT(slotSendCalibrationData()));
}

void cvCal::slotCalcNotes()
{
    uint8_t cv, octave, note, noteIndex;
    uint16_t step;

    slotUpdateCVcalData();

    for (cv = 0; cv < NUM_CV_OUTS; cv++)
    {
        for (octave = 0; octave < NUM_CV_OCTAVES - 1; octave++)
        {
            // Special handling for the low octave calibration value being meant for C#
            if (octave == 0)
            {
                noteIndex = 0;
                cvCalData.data.notes[cv][noteIndex++] = 0; // This is C
                cvCalData.data.notes[cv][noteIndex++] = cvCalData.data.octaves[cv][0]; // This is C#

                step = (cvCalData.data.octaves[cv][1] - cvCalData.data.octaves[cv][0]) / 11; // for C#, 11 divisions
                for (note = 2; note < 12; note++)
                {
                    // Start from C# to B
                    noteIndex = note; // Direct mapping here
                    cvCalData.data.notes[cv][noteIndex] = cvCalData.data.notes[cv][0] + (step * note);
                }
            }
            else
            {
                // Calculate the voltage step per note for 1V-5V
                step = (cvCalData.data.octaves[cv][octave + 1] - cvCalData.data.octaves[cv][octave]) / 12;
                for (note = 0; note < 12; note++)
                {
                    noteIndex = octave * 12 + note;
                    if (noteIndex >= NUM_CV_NOTES) break; // Safety check
                    cvCalData.data.notes[cv][noteIndex] = cvCalData.data.octaves[cv][octave] + (step * note);
                }
            }
        }

        qDebug() << "cv: " << cv << " note60: " << cvCalData.data.octaves[cv][5];
        cvCalData.data.notes[cv][++noteIndex] = cvCalData.data.octaves[cv][5]; // highest C = 5V
    }

    slotUpdateUiVals();
}


void cvCal::slotValueChanged()
{

    if(QObject::sender())
    {
        QObject *sender = QObject::sender();
        QString jsonName = sender->objectName();
        int value;

         qDebug() << "slotValueChanged: " << sender->metaObject()->className();

        if(sender->metaObject()->className() == QString("kmiSpinBoxUpDown"))
        {

            kmiSpinBoxUpDown* spinbox = qobject_cast<kmiSpinBoxUpDown *>(sender);
            value = spinbox->value();

            int dacChannel = 0;

            if (jsonName.contains("cv1"))
            {
                dacChannel = 1;
            }
            else if (jsonName.contains("cv2"))
            {
                dacChannel = 2;
            }

            qDebug() << "kmiSpinBoxUpDown: " << jsonName << " val: " << value;

            emit signalSendNRPN(dacChannel, value, nrpnChannel);
        }

        if(sender->metaObject()->className() == QString("QComboBox"))
        {
            QComboBox* comboBox = qobject_cast<QComboBox *>(sender);
            QString mode = comboBox->currentText();

            if (mode == "Octaves")
            {
                cvCalData.data.cal_mode = CV_CAL_MODE_OCTAVES;
                ui->modeStack->setCurrentIndex(0);
                ui->button_calcNotes->hide();
            }
            else if (mode == "Notes")
            {
                cvCalData.data.cal_mode = CV_CAL_MODE_NOTES;
                ui->modeStack->setCurrentIndex(1);
                ui->button_calcNotes->show();
            }
        }
    }
}

void cvCal::slotUpdateNRPNChannel(int channel)
{
    qDebug() << "slotUpdateNRPNChannel called - channel: " << channel;

    if (channel == 16) // index 16 = disabled
    {
        channel = MIDI_CH_16; // default backup channel, matches firmware
    }
    nrpnChannel = channel;
}

#include <QtEndian>

uint16_t cvCal::get16bit(uint8_t msb, uint8_t lsb)
{
    return ((msb << 8) | lsb) & 0xFFFF; // Ensure the result is a 16-bit integer
}

uint16_t cvCal::reverseBytes(uint16_t value) {
    return (value >> 8) | (value << 8);
}

void cvCal::slotUpdateUiVals()
{
    slotDisconnectElements();

    ui->cv1_008v->setValue(cvCalData.data.octaves[CV_OUT1][0]);
    ui->cv1_1v->setValue(cvCalData.data.octaves[CV_OUT1][1]);
    ui->cv1_2v->setValue(cvCalData.data.octaves[CV_OUT1][2]);
    ui->cv1_3v->setValue(cvCalData.data.octaves[CV_OUT1][3]);
    ui->cv1_4v->setValue(cvCalData.data.octaves[CV_OUT1][4]);
    ui->cv1_5v->setValue(cvCalData.data.octaves[CV_OUT1][5]);

    ui->cv2_008v->setValue(cvCalData.data.octaves[CV_OUT2][0]);
    ui->cv2_1v->setValue(cvCalData.data.octaves[CV_OUT2][1]);
    ui->cv2_2v->setValue(cvCalData.data.octaves[CV_OUT2][2]);
    ui->cv2_3v->setValue(cvCalData.data.octaves[CV_OUT2][3]);
    ui->cv2_4v->setValue(cvCalData.data.octaves[CV_OUT2][4]);
    ui->cv2_5v->setValue(cvCalData.data.octaves[CV_OUT2][5]);

    for (int c = 0; c < cvCalData.NumCVOuts; c++)
    {
        for (int i = 0; i < cvCalData.NumCVNotes; i++)
        {
            QString name = QString("cv%1_N_%2").arg(c+1).arg(i); // object name isn't 0 indexed
            kmiSpinBoxUpDown* spinBox = this->findChild<kmiSpinBoxUpDown*>(name);
            spinBox->setValue(cvCalData.data.notes[c][i]);
        }
    }

    slotConnectElements();
}

void cvCal::slotParseDeviceCVCalibration(uint8_t *src, uint16_t length)
{
    qDebug() << "slotParseDeviceCVCalibration called - length: " << length << " CV_CALIBRATION: " << sizeof(CV_CALIBRATION);
    if (length != sizeof(CV_CALIBRATION))
    {
        qDebug() << "SysEx payload does not match CV_CALIBRATION struct, halting";
        return;
    }

    // update header
    cvCalData.data.version = *src++; length--;
    cvCalData.data.cal_mode = *src++; length--;

    if (cvCalData.data.cal_mode == CV_CAL_MODE_NOTES)
    {
        ui->cv_cal_mode->setCurrentText("Notes");
        ui->modeStack->setCurrentIndex(1);
    }
    else // default to CV_CAL_MODE_OCTAVES, also accepts factory
    {
        ui->cv_cal_mode->setCurrentText("Octaves");
        ui->modeStack->setCurrentIndex(0);
    }


    if (cvCalData.data.version != CURRENT_CV_CAL_VERSION)
    {
        qDebug() << "ERROR: cvCal version mismatch - received: " << cvCalData.data.version << " expected: " << CURRENT_CV_CAL_VERSION;
    }

    qDebug() << "cal_mode: " << calModeMap.key(cvCalData.data.cal_mode);

    /*
     * Incoming 16bit data is ordered in the array MSB then LSB (0,60 = 60).
     * First we test the endianness of our system for portability, then write
     * to the correct pointer array that orders the incoming bytes correctly.
    */

    if (cvCalData.systemIsLittleEndian())
    {
        for (int i = 0; i < length; i++)
        {
            *(cvCalData.data_bytestreamMSBthenLSB[i]) = *src++; // order the bytestream MSB first
        }
    }
    else
    {
        for (int i = 0; i < length; i++)
        {
            *(cvCalData.data_bytestreamLSBthenMSB[i]) = *src++; // flip lsb/msb order for big endian systems
        }
    }

    slotUpdateUiVals();
}


void cvCal::slotGetDeviceCVCalibration()
{
    qDebug() << "slotGetDeviceCVCalibration called";

    emit signalSendStepSXPacket(MSG_CAT_CALIBRATION, REQUEST_CV_CAL, 0, 0);

}

void cvCal::slotResetDeviceCVCalibration()
{
    qDebug() << "slotResetDeviceCVCalibration called";

    emit signalSendStepSXPacket(MSG_CAT_CALIBRATION, RESET_CV_CAL_TO_FACTORY, 0, 0);
}

void cvCal::slotUpdateCVcalData()
{
    // update data
    cvCalData.data.version = CURRENT_CV_CAL_VERSION;

    QString mode = ui->cv_cal_mode->currentText();

    if (mode == "Octaves")
    {
        cvCalData.data.cal_mode = CV_CAL_MODE_OCTAVES;
    }
    else if (mode == "Notes")
    {
        cvCalData.data.cal_mode = CV_CAL_MODE_NOTES;
    }

    cvCalData.data.octaves[CV_OUT1][0] = (ui->cv1_008v->value());
    cvCalData.data.octaves[CV_OUT1][1] = (ui->cv1_1v->value());
    cvCalData.data.octaves[CV_OUT1][2] = (ui->cv1_2v->value());
    cvCalData.data.octaves[CV_OUT1][3] = (ui->cv1_3v->value());
    cvCalData.data.octaves[CV_OUT1][4] = (ui->cv1_4v->value());
    cvCalData.data.octaves[CV_OUT1][5] = (ui->cv1_5v->value());

    cvCalData.data.octaves[CV_OUT2][0] = (ui->cv2_008v->value());
    cvCalData.data.octaves[CV_OUT2][1] = (ui->cv2_1v->value());
    cvCalData.data.octaves[CV_OUT2][2] = (ui->cv2_2v->value());
    cvCalData.data.octaves[CV_OUT2][3] = (ui->cv2_3v->value());
    cvCalData.data.octaves[CV_OUT2][4] = (ui->cv2_4v->value());
    cvCalData.data.octaves[CV_OUT2][5] = (ui->cv2_5v->value());

    for (int c = 0; c < cvCalData.NumCVOuts; c++)
    {
        for (int i = 0; i < cvCalData.NumCVNotes; i++)
        {
            QString name = QString("cv%1_N_%2").arg(c+1).arg(i); // object name isn't 0 indexed
            kmiSpinBoxUpDown* spinBox = this->findChild<kmiSpinBoxUpDown*>(name);
            cvCalData.data.notes[c][i] = spinBox->value();
        }
    }
}

void cvCal::slotSendCalibrationData()
{
    qDebug() << "slotSendCalibrationData called";

    slotUpdateCVcalData();

    uint8_t txPayload[CV_CALDATA_ARRAYSIZE];
    int payloadIndex = 0;

    txPayload[payloadIndex++] = cvCalData.data.version;
    txPayload[payloadIndex++] = cvCalData.data.cal_mode;

    if (cvCalData.systemIsLittleEndian())
    {
        for (int i = 0; i < cvCalData.arraySize - 2; i++)
        {
            txPayload[payloadIndex++] = *(cvCalData.data_bytestreamMSBthenLSB[i]); // prep the bytestream
        }
    }
    else
    {
        for (int i = 0; i < cvCalData.arraySize - 2; i++)
        {
            txPayload[payloadIndex++] = *(cvCalData.data_bytestreamLSBthenMSB[i]); // reverse MSB/LSB order for big endian systems
        }
    }

    emit signalSendStepSXPacket(MSG_CAT_CALIBRATION, CV_CAL_PAYLOAD, &txPayload[0], cvCalData.arraySize);

    //close();
}
