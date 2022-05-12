// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef TROUBLESHOOT_H
#define TROUBLESHOOT_H

#include <QDialog>
#include <QWidget>
#include <QFile>
#include <QObject>
#include <QtWidgets>

namespace Ui {
class troubleshoot;
}

class troubleshoot : public QWidget
{
    Q_OBJECT

public:
    explicit troubleshoot(QWidget *parent = nullptr, QString initDeviceName = "None", QString initFwVer = "0.0.0");
    ~troubleshoot();

    QWidget *troubleshootWidget;

    QString deviceName, appFwVer;

    bool connected;
    bool bootloader;
    bool suppressStatus;

    //Button styles
    QFile* blueStyleFile;
    QString blueStyleString;
    QFile* grayStyleFile;
    QString grayStyleString;

    //

    // functions
    QString timeStamp();

public slots:
    void slotInputCount(int count);
    void slotOutputCount(int count);
    void slotInputPort(QString portName, int port);
    void slotOutputPort(QString portName, int port);

    void slotDetected();
    void slotBootloaderMode();
    void slotConnected(bool status);
    void slotSetDevVersion(QString fwVersion, QString blVersion);
    void slotRequestFwUpdate();
    void slotFirmwareUpdated(bool success);

    void slotAppendToStatusLog(QString message);
    void slotGatherReport();
    void slotScrollTroubleUp();

private:
    Ui::troubleshoot *ui;
};

#endif // TROUBLESHOOT_H
