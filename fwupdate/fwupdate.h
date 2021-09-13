// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef FWUPDATE_H
#define FWUPDATE_H

#include <QDialog>
#include <QWidget>
#include <QFile>
#include <QObject>
#include <QtWidgets>

namespace Ui {
class fwUpdate;
}

class fwUpdate : public QDialog
{
    Q_OBJECT

public:
    explicit fwUpdate(QWidget *parent = nullptr, QString initDeviceName = "None", QString initFwVer = "0.0.0");
    ~fwUpdate();

    QString deviceName, appFwVer;

    //Buttons
    QFile* blueStyleFile;
    QString blueStyleString;

    //Buttons
    QFile* grayStyleFile;
    QString grayStyleString;

    bool updateSuccessful;

signals:

    void signalRequestFwUpdate();
    void signalFwUpdateSuccess();
    void signalFwUpdateSuccessCloseDialog(bool);

public slots:

    void slotRequestFwUpdate();
    void slotAppendTextToConsole(QString thisText);
    void slotUpdateProgressBar(int thisPercent);
    void slotClearText();
    void slotFwUpdateTimeout();
    void slotFwUpdateComplete(bool success);
    void closeEvent(QCloseEvent *event);

private:
    Ui::fwUpdate *ui;
};

#endif // FWUPDATE_H
