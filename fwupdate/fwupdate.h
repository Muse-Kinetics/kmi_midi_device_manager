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

#define FW_WIN_WIDTH 500
#define FW_WIN_HEIGHT 350
#define FW_WIN_X_CENTER FW_WIN_WIDTH / 2
#define FW_WIN_Y_CENTER FW_WIN_HEIGHT / 2

#define FW_BUTT_WIDTH 80
#define FW_BUTT_HEIGHT 24
#define FW_BUTT_PADDING 8
#define FW_BUTT_X_CENTER FW_BUTT_WIDTH / 2
#define FW_BUTT_Y_CENTER FW_BUTT_HEIGHT / 2
#define FW_BUTT_COL1 FW_WIN_X_CENTER - (FW_BUTT_WIDTH + FW_BUTT_PADDING)
#define FW_BUTT_COL2 FW_WIN_X_CENTER + (FW_BUTT_WIDTH + FW_BUTT_PADDING)
#define FW_BUTT_ROW1 310


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

    //Button styles
    QFile* blueStyleFile;
    QString blueStyleString;
    QFile* grayStyleFile;
    QString grayStyleString;

    bool updateSuccessful;

signals:

    void signalRequestFwUpdate();
    void signalFwUpdateSuccess();
    void signalFwUpdateSuccessCloseDialog(bool);

public slots:

    void slotPressButtDone();
    void slotPressButtOk();
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
