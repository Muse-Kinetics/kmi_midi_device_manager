// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include <fwupdate.h>
#include "ui_fwupdate.h"
#include <QApplication>

fwUpdate::fwUpdate(QWidget *parent, QString initDeviceName, QString initAppFwVer) :
    QDialog(parent),
    ui(new Ui::fwUpdate)
{
    deviceName = initDeviceName;
    appFwVer = initAppFwVer;

    updateSuccessful = false;

    qDebug() << "Create new fwUpdate window: " << deviceName;

    //StyleSheets for Buttons
    blueStyleFile = new QFile(":/stylesheets/RedButtonStyleSheet.qss"); // eb todo - fix this in quneo/qunexus
    blueStyleFile->open(QFile::ReadOnly);
    blueStyleString = QLatin1String(blueStyleFile->readAll());

    grayStyleFile = new QFile(":/stylesheets/GrayButtonStyleSheet.qss"); // eb todo - fix this in quneo/qunexus
    grayStyleFile->open(QFile::ReadOnly);
    grayStyleString = QLatin1String(grayStyleFile->readAll());

    ui->setupUi(this);

    ui->stack->setCurrentIndex(0);
    ui->progressBar->setValue(0);
    ui->console->setReadOnly(true);

    // scroll to bottom automatically
    ui->console->ensureCursorVisible();
    ui->console->verticalScrollBar()->setValue(ui->console->verticalScrollBar()->maximum());

    qDebug() << "apply stylesheets to buttons";

    ui->butt_done->hide();
    ui->butt_done->setStyleSheet(blueStyleString);
    ui->butt_done->setAutoDefault(false);

    ui->butt_cancel->setStyleSheet(grayStyleString);
    ui->butt_cancel->setAutoDefault(false);

    ui->butt_ok->setStyleSheet(blueStyleString);
    ui->butt_ok->setAutoDefault(false);

    ui->progressBar->setTextVisible(false);

    //ui->butt_retry->hide();
    //ui->butt_retry->setStyleSheet(blueStyleString);
    //ui->butt_retry->setAutoDefault(false);

    this->setWindowTitle(deviceName);

    connect (ui->butt_cancel, SIGNAL(clicked()), this, SLOT(close()));
    connect (ui->butt_done, SIGNAL(clicked()), this, SLOT(close()));
    connect (ui->butt_ok, SIGNAL(clicked()), this, SLOT(slotRequestFwUpdate()));
    //connect (ui->butt_retry, SIGNAL(clicked()), this, SLOT(slotRequestFwUpdate()));

#ifdef Q_OS_WIN

    QString fwUpdateHTMLFile = ":/stylesheets/fwupdate_WIN.html";
    QFile fwUpdateHTML(fwUpdateHTMLFile);

    if (fwUpdateHTML.open(QFile::ReadOnly))
    {
        QString fwUpdateString = QLatin1String(fwUpdateHTML.readAll());
        ui->textBrowser->setHtml(fwUpdateString);
    }
    else
    {
        qDebug() << "ERROR - could not find fwUpdate HTML file: " << fwUpdateHTMLFile;
    }

    //ui->textBrowser->setStyleSheet("background-color: black;color: white;");
    //ui->console->setStyleSheet("background-color: black;color: white; font: 9pt \"Arial\";");
#endif
}

fwUpdate::~fwUpdate()
{
    delete ui;
}

void fwUpdate::slotRequestFwUpdate()
{
    ui->interrupt_warning->show();
    ui->stack->setCurrentIndex(1); // goto the next page
    emit signalRequestFwUpdate(); // alert app to update
    qDebug() << "fwupdate window - emit signalRequestFwUpdate";
}

void fwUpdate::slotAppendTextToConsole(QString thisText)
{
    qDebug() << "slotAppendTextToConsole called - thisText: " << thisText;
    ui->console->insertPlainText(thisText);
    ui->console->verticalScrollBar()->setValue(ui->console->verticalScrollBar()->maximum());

//    QTextCursor c = ui->console->textCursor();
//    c.movePosition(QTextCursor::End);
//    ui->console->setTextCursor(c);
}

void fwUpdate::slotUpdateProgressBar(int thisPercent)
{
    ui->progressBar->setValue(thisPercent);
}

void fwUpdate::slotClearText()
{
    qDebug() << "slotClearText called";
    ui->console->clear();
}

void fwUpdate::slotFwUpdateTimeout()
{
    ui->interrupt_warning->hide();
    //ui->butt_done->setGeometry(FW_BUTT_COL1, FW_BUTT_ROW1, FW_BUTT_WIDTH, FW_BUTT_HEIGHT); // move "done" button to the left
    ui->butt_done->setText("Abort"); // change text but for now keep as a "successful" exit, which should re-send connection checks
    ui->butt_done->show();
    //ui->butt_retry->show();

}

void fwUpdate::slotFwUpdateComplete(bool success)
{
    if (success)
    {
        qDebug() << "updateSuccessful = true";
        updateSuccessful = true;
        slotUpdateProgressBar(100);

        int fwLength = (deviceName == "QuNeo") ? 8 : appFwVer.length(); // fix for quneo odd length firmware
        slotAppendTextToConsole("\nFirmware successfully updated to " + appFwVer.right(fwLength) + "\n");
#ifdef Q_OS_WINDOWS
        slotAppendTextToConsole("\nThe application will now re-launch");
#endif
        ui->interrupt_warning->hide();

        //ui->butt_retry->hide();

        ui->butt_done->setText("Done"); // change text but for now keep as a "successful" exit, which should re-send connection checks
        //ui->butt_done->setGeometry(FW_WIN_X_CENTER - FW_BUTT_X_CENTER, FW_BUTT_ROW1, FW_BUTT_WIDTH, FW_BUTT_HEIGHT); // move "done" button to the left
        ui->butt_done->show();

        emit signalFwUpdateSuccess();
    }
    else
    {
        updateSuccessful = false;
        ui->console->insertPlainText("\nFirmware update failed.\nPlease try again, and if you continue to have issues, copy/paste this log and open a support ticket at:\n\nhttps://support.keithmcmillen.com ");
        ui->console->verticalScrollBar()->setValue(ui->console->verticalScrollBar()->maximum());
        slotFwUpdateTimeout();
    }
}

void fwUpdate::closeEvent(QCloseEvent *event)
{
    qDebug() << "closeEvent";

    if (updateSuccessful)
    {
        qDebug() << "SuccessClose";
        emit signalFwUpdateSuccessCloseDialog(true);
        ui->butt_done->hide();
        ui->stack->setCurrentIndex(0);
        slotClearText();
    }
    else
    {
        qDebug() << "FailClose";
        emit signalFwUpdateSuccessCloseDialog(false);

        // restore window
        //ui->butt_retry->hide();
        ui->butt_done->hide();

        ui->butt_done->setText("Done"); // change text but for now keep as a "successful" exit, which should re-send connection checks
        //ui->butt_done->setGeometry(FW_BUTT_COL1, FW_BUTT_ROW1, FW_BUTT_WIDTH, FW_BUTT_HEIGHT); // move "done" button to the middle

        ui->interrupt_warning->show();
        ui->stack->setCurrentIndex(0);
        slotClearText();


    }
    updateSuccessful = false; // reset
    event->accept();
}



