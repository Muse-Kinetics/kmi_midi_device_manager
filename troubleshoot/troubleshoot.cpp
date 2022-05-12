// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "troubleshoot.h"
#include "ui_troubleshoot.h"
#include <QDateTime>

troubleshoot::troubleshoot(QWidget *parent, QString initDeviceName, QString initAppFwVer) :
    QWidget(parent),
    ui(new Ui::troubleshoot)
{
    qDebug() << "Create new troubleshoot window: " << deviceName;

    this->setWindowFlags(Qt::Window | Qt::WindowCloseButtonHint | Qt::CustomizeWindowHint);
    this->setWindowModality(Qt::NonModal);
    this->setFixedSize(1000,480);
    this->setWindowTitle("Diagnostics");

    this->hide();
    qDebug() << "setup widget";

    suppressStatus = false;

    //-------- Setup Ui
    ui->setupUi(this);

    qDebug() << "setup windows";

    ui->statusWindow->setReadOnly(true);
    ui->portsWindow->setReadOnly(true);
    ui->troubleWindow->setReadOnly(true);

    // scroll to bottom automatically
    ui->statusWindow->ensureCursorVisible();
    ui->statusWindow->verticalScrollBar()->setValue(ui->portsWindow->verticalScrollBar()->maximum());

    deviceName = initDeviceName;
    appFwVer = initAppFwVer;

    qDebug() << "Create new troubleshoot window: " << deviceName;

    //StyleSheets for buttons
    QString bsf = ":/stylesheets/RedButtonStyleSheet.qss";
    blueStyleFile = new QFile(bsf);
    if (!blueStyleFile->open(QFile::ReadOnly)) qDebug() << "Error: could not find stylesheet: " << bsf;
    blueStyleString = QLatin1String(blueStyleFile->readAll());

    QString gsf = ":/stylesheets/GrayButtonStyleSheet.qss";
    grayStyleFile = new QFile(gsf);
    if (!grayStyleFile->open(QFile::ReadOnly)) qDebug() << "Error: could not find stylesheet: " << gsf;
    grayStyleString = QLatin1String(grayStyleFile->readAll());


    qDebug() << "apply stylesheets to buttons";

    ui->butt_close->setStyleSheet(blueStyleString);
    ui->butt_close->setAutoDefault(false);

    this->setWindowTitle("Troubleshooting");

    connect(ui->butt_gather, SIGNAL(clicked()), this, SLOT(slotGatherReport()));
    connect(ui->butt_close, SIGNAL(clicked()), this, SLOT(close()));

    ui->statusWindow->clear();
    slotAppendToStatusLog(QString("Log initialized - %1").arg(appFwVer.left(appFwVer.length() - 2)));
    ui->troubleWindow->clear();
    ui->troubleWindow->append(QString("The %1 MIDI port has not been detected by the operating system. If you have plugged in the device and still see this message, try the following:\n\n"
                                      "1) Disconnect %1.\n"
                                      "2) Close all other applications.\n"
                                      "3) Close and then re-open the %1 editor.\n"
                                      "\n"
                                      "Before proceeding to the next step, make sure that you are using a USB cable that you know works. Test it with other USB MIDI devices and verify that power and data work. When in doubt, try swapping the cable.\n"
                                      "\n"
                                      "4) Reconnect %1 directly to the computer, do not use a USB hub.\n"
                                      "\n"
                                      "If %1 is still not detected:\n"
                                      "\n"
                                  #ifdef Q_OS_WIN
                                      "5) Open the Windows Device Manager by holding down the Windows key and pressing R, and then typing \"devmgmt.msc\" and pressing enter.\n"
                                      "6) Scroll down and double click \"Sound, video and game controllers\". %1 should be listed, double click it and verify that the Device Status says \"This device is working properly.\"\n"
                                      "7) If you're still having issues connecting, try right clicking %1 and selecting \"Update Driver\". If that doesn't work, try \"Uninstall device\" and rebooting.\n"
                                  #else
                                      "5) Open Audio Midi Setup by holding down the Command key and pressing the Space Bar, and then typing \"Audio MIDI Setup\" and pressing enter.\n"
                                      "6) Open the MIDI Studio by holding down the Command key and pressing 2. %1 should be displayed as a square icon, it should not be greyed out. Double click the icon and verify that the \"Device is online\" box is checked.\n"
                                      "7) If you're still having issues connecting, try selecting the %1 icon and pressing delete, then reboot your computer.\n"
                                  #endif
                                      "8) If none of the above works, contact support and copy/paste the diagnostic report into your support ticket.").arg(deviceName));
    QTimer::singleShot(100, this, SLOT(slotScrollTroubleUp()));

    // fix windows that are not on the screen
    int posX = this->mapToGlobal(QPointF(0,0)).x();
    int posY = this->mapToGlobal(QPointF(0,0)).y();


    qDebug() << "thisPosition - x: " << posX << " y: " << posY;
//    if (posY < 0)
//    {
//        posY = 0;
//    }

    this->move(posX+1, posY+1);
}

troubleshoot::~troubleshoot()
{
    delete ui;
}


void troubleshoot::slotInputCount(int count)
{
    ui->portsWindow->clear();
    ui->portsWindow->append(QString("Number of MIDI Input Ports: %1\n").arg(count));
}

void troubleshoot::slotOutputCount(int count)
{
    ui->portsWindow->append(QString("\nNumber of MIDI Output Ports: %1\n").arg(count));
}

void troubleshoot::slotInputPort(QString portName, int port)
{
    ui->portsWindow->append(QString("Input port #%1: %2").arg(port).arg(portName));
}

void troubleshoot::slotOutputPort(QString portName, int port)
{
    ui->portsWindow->append(QString("Output port #%1: %2").arg(QString::number(port), portName));
}

void troubleshoot::slotDetected()
{
    slotAppendToStatusLog(QString("%1 MIDI port has been detected.").arg(deviceName));
    ui->troubleWindow->clear();
    ui->troubleWindow->append(QString("%1 has not responded to the firmware version request. \n"
                                      "1) Disconnect %1.\n"
                                      "2) Close all other applications.\n"
                                      "3) Close and then re-open the %1 editor.\n"
                                      "4) Reconnect %1 directly to the computer, do not use a USB hub.\n"
                                      "5) If %1 still doesn't respond, contact support and copy/paste the diagnostic report into your support ticket.").arg(deviceName));
}

void troubleshoot::slotBootloaderMode()
{
    slotAppendToStatusLog(QString("%1 has been detected, and is in bootloader mode.").arg(deviceName));
    ui->troubleWindow->clear();
    ui->troubleWindow->append(QString("%1 is connected and in bootloader mode.\n\n"
                                      "1) If you haven't already been prompted to update, go to the \"Hardware\" menu and select \"Update Firmware...\"\n"
                                      "2) Follow the prompts to complete the update.\n").arg(deviceName));
}

void troubleshoot::slotConnected(bool status)
{
    connected = status;

    if (status)
    {
        slotAppendToStatusLog(QString("%1 is connected and the detected firmware is compatible with the editor.").arg(deviceName));
        ui->troubleWindow->clear();
        ui->troubleWindow->append("None");
    }
    else
    {
        slotAppendToStatusLog(QString("%1 disconnected.").arg(deviceName));
    }
}

void troubleshoot::slotSetDevVersion(QString fwVersion, QString blVersion)
{
    slotAppendToStatusLog(QString("Response: %1").arg(blVersion.left(blVersion.length() - 2)));
    slotAppendToStatusLog(QString("Response: %1").arg(fwVersion.left(fwVersion.length())));
    ui->troubleWindow->clear();
    ui->troubleWindow->append(QString("%1 has responded to the firmware version request. One of two things should happen next:\n\n"
                                      "A) The Application Firmware Version and the Device Firmware Versions match, the editor should connect.\n"
                                      "or\n"
                                      "B) The Application and Device firmware versions do not match, the editor will prompt you to update the firmware.\n"
                                      "\n"
                                      "If neither is the case, contact support and copy/paste the diagnostic report into your support ticket.").arg(deviceName));
    QTimer::singleShot(20, this, SLOT(slotScrollTroubleUp()));
}

void troubleshoot::slotRequestFwUpdate()
{
    if (suppressStatus = true)
    {
        suppressStatus = false;
    }
    else
    {
        slotAppendToStatusLog("Beginning firmware update process.");
    }
    ui->troubleWindow->clear();
    ui->troubleWindow->append(QString("The editor is attempting to update the %1 firmare. If the process does not complete successfully:\n\n"
                                      "1) Disconnect %1.\n"
                                      "2) Close all other applications.\n"
                                      "3) Close and then re-open the %1 editor.\n"
                                      "4) Reconnect %1 directly to the computer, do not use a USB hub.\n"
                                      "5) Retry the firmware update process.\n"
                                      "6) If the firmware update process fails again, contact support and copy/paste the diagnostic report into your support ticket.\n").arg(deviceName));
    QTimer::singleShot(20, this, SLOT(slotScrollTroubleUp()));
}

void troubleshoot::slotFirmwareUpdated(bool success)
{
    slotAppendToStatusLog(QString("%1 firmware update - success: %2").arg(deviceName, QString::number(success)));
    ui->troubleWindow->clear();
    if (success)
    {
        ui->troubleWindow->append("None");
    }
    else
    {
        ui->troubleWindow->append(QString("The firmware update has failed.\n\n"
                                          "1) Disconnect %1.\n"
                                          "2) Close all other applications.\n"
                                          "3) Close and then re-open the %1 editor.\n"
                                          "4) Reconnect %1 directly to the computer, do not use a USB hub.\n"
                                          "5) Follow the prompts to attempt the firmware update again.\n"
                                          "6) If %1 still doesn't respond, contact support and copy/paste the diagnostic report into your support ticket.").arg(deviceName));

    }
}

void troubleshoot::slotAppendToStatusLog(QString message)
{
    //qDebug() << "slotAppendToStatusLog called - message: " << message;

    if (message == "\nDevice bootloader detected.\n" ||
        message == "\nUpdating Firmware...\n")
    {
        suppressStatus = true;
        slotRequestFwUpdate();
    }
    if (message.left(1) == "\n")
    {
        //qDebug() << "Removing line break";
        message = message.mid(1, message.length() - 2);
        if (message.left(1) == "\n")
        {
            //qDebug() << "Removing 2nd line break";
            message = message.mid(1, message.length() - 2);
        }
    }
//    else
//    {
//        qDebug() << "did not remove: " << message.left(1);
//    }
    ui->statusWindow->append(QString("[%1]: %2").arg(timeStamp(), message));
}

QString troubleshoot::timeStamp()
{
    QDateTime current = QDateTime::currentDateTime();
    QString currentTime = current.time().toString("hh:mm:ss:zzz");
    return currentTime;
}

void troubleshoot::slotGatherReport()
{
    QClipboard *clipboard = QGuiApplication::clipboard();

    QString report = QString("Status:\n"
                             "\n"
                             "%1\n"
                             "\n"
                             "MIDI Ports:\n"
                             "\n"
                             "%2\n"
                             "\n"
                             "Troubleshooting:\n"
                             "%3").arg(ui->statusWindow->toPlainText(), ui->portsWindow->toPlainText(), ui->troubleWindow->toPlainText());
    clipboard->setText(report);
}

void troubleshoot::slotScrollTroubleUp()
{
    qDebug() << "slotScrollTroubleUp called - troubleScroll value: " <<  ui->troubleWindow->verticalScrollBar()->value();
    ui->troubleWindow->verticalScrollBar()->setValue(0);
    qDebug() << "value set - troubleScroll value: " <<  ui->troubleWindow->verticalScrollBar()->value();
}
