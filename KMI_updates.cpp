// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "KMI_updates.h"

KMI_Updates::KMI_Updates(QWidget *p, QString an, QSettings *_sessionSettings, QByteArray av, QString initURL)
{
    // initialize
    sessionSettings = _sessionSettings;
    parent = p;
    jsonVersionURL = initURL;


    applicationVersion.resize(3);
    applicationVersion[0] = uchar(av.at(0));
    applicationVersion[1] = uchar(av.at(1));
    applicationVersion[2] = uchar(av.at(2));

    networkAccessManager = new(QNetworkAccessManager);
    //networkAccessManager->set


    appName = an;

    connect(networkAccessManager, SIGNAL(finished(QNetworkReply*)), this, SLOT(slotUpdateCheckReply(QNetworkReply*)));

    manualUpdateCheck = false;

    qDebug() << "KMI Updates Module initialized, application version: " << uchar(applicationVersion[0]) << "." << uchar(applicationVersion[1]) << "." << uchar(applicationVersion[2]);

    slotCheckForUpdates();

}

void KMI_Updates::slotManualCheckForUpdates()
{
    //qDebug() << "slotManualCheckForUpdates called";

    manualUpdateCheck = true;
    slotCheckForUpdates();
}

void KMI_Updates::slotCheckForUpdates()
{
    //qDebug() << "slotCheckForUpdates called";

    //QSslSocket::setProtocol()
    qDebug() << "SSL Check: " << QSslSocket::supportsSsl() << QSslSocket::sslLibraryBuildVersionString() << QSslSocket::sslLibraryVersionString();
    qDebug() << "checking for updates";
    networkAccessManager->get(QNetworkRequest(QUrl(jsonVersionURL)));

}

void KMI_Updates::slotUpdateCheckReply(QNetworkReply *networkReply)
{
    //qDebug() << "slotUpdateCheckReply called";
    QMessageBox m(QMessageBox::NoIcon, "Software Update", "", QMessageBox::NoButton, 0, Qt::Dialog);

    //m.setStyleSheet("QPushButton {font-family: \"Arial\";font-size: 10px;background-color: rgba(162, 0, 0, 255);border-style: outset;border-radius: 4.0;border-width: 1px;border-color: rgba(0, 51, 76, 255);color: white;} QPushButton:pressed {background-color: rgba(122, 0, 0, 255);} QPushButton:focus  outline: none;}");

    QString messageBoxText;

    if(networkReply->isFinished() && networkReply->error() == QNetworkReply::NoError)
    {
        QByteArray replyArray = networkReply->readAll();
        QJsonDocument jsonDoc = QJsonDocument::fromJson(replyArray);
        QVariantMap versionMap = jsonDoc.toVariant().toMap();

        QByteArray JSONVersion = versionMap.value("Editor").toByteArray();
        int foundVersion[3];


        int firstDecimal = JSONVersion.indexOf(".");
        int secondDecimal = JSONVersion.indexOf(".", firstDecimal + 1);

        foundVersion[0] = JSONVersion.left(firstDecimal).toInt();
        foundVersion[1] = JSONVersion.mid(firstDecimal + 1, secondDecimal - firstDecimal - 1).toInt();
        foundVersion[2] = JSONVersion.mid(secondDecimal + 1, 2).toInt();

        qDebug() << "foundVersion: " << foundVersion[0] << "." << foundVersion[1] << "." << foundVersion[2] << " applicationVersion:" << uchar(applicationVersion[0]) << uchar(applicationVersion[1]) << uchar(applicationVersion[2]);;

        bool appIsOutOfDate = false;

        if ((uchar(foundVersion[0])) > uchar(applicationVersion[0]) ) // major version is greater
        {
            appIsOutOfDate = true;
        }
        else if (uchar(foundVersion[0]) == uchar(applicationVersion[0])) // major version is the same
        {
            if (uchar(foundVersion[1]) > uchar(applicationVersion[1])) // middle version is greater
            {
                appIsOutOfDate = true;
            }
            else if (uchar(foundVersion[1]) == uchar(applicationVersion[1])) // middle version is the same
            {
                if (uchar(foundVersion[2]) > uchar(applicationVersion[2])) // minor version is greater
                {
                    appIsOutOfDate = true;
                }
            }
        }

        if (appIsOutOfDate)
        {
            bool skipUpdate = slotReturnSkipUpdateBool(JSONVersion);
            qDebug() << "manualUpdateCheck: " << manualUpdateCheck << "skipUpdate: " << skipUpdate;
            if(manualUpdateCheck || !skipUpdate)
            {
                QString updateMsg = versionMap.value("message").toString();
                messageBoxText = QString("%1 version %2.%3.%4 is available.").arg(appName).arg(foundVersion[0]).arg(foundVersion[1]).arg(foundVersion[2]);
                m.setTextFormat(Qt::RichText);
                m.setDetailedText(updateMsg);
                m.setText(messageBoxText);
                qDebug() << "update - updateMsg: " << updateMsg;
                m.addButton("Download", QMessageBox::YesRole);
                m.addButton("Skip This Version", QMessageBox::NoRole);
                m.addButton("Remind Me Later", QMessageBox::RejectRole);

                int exitCode = m.exec();

                if(exitCode == 0)
                {
                    slotGoToDownloadsPage();
                }
                else if(exitCode == 1)
                {
                    slotSkipVersion(JSONVersion);
                }
                else //in order to get it to actually remind you later, I'm reverting the skip version to default
                {
                    slotSkipVersion("0");
                }
            }
        }
        else
        {
            qDebug() << "Editor version exceeds or matches kmi website";
            if(manualUpdateCheck) //only display the dialog in this situation if we're checking manually
            {
                QString foundVersionString = versionMap.value("Editor").toString();
                messageBoxText = QString("%1 is Up To Date.").arg(appName);
                messageBoxText = QString("%1\n\nApplication Version: %2.%3.%4").arg(messageBoxText).arg(uchar(applicationVersion[0])).arg(uchar(applicationVersion[1])).arg(uchar(applicationVersion[2]));
                messageBoxText = QString("%1\nKMI Website Version: %2").arg(messageBoxText).arg(foundVersionString);


                m.setText(messageBoxText);
                m.addButton("OK", QMessageBox::AcceptRole);
                m.exec();
                manualUpdateCheck = false;
            }
        }

    }
    else
    {
        qDebug() << "software update error:" << networkReply->error();
        if(manualUpdateCheck) //only show the error if update is happening manually
        {
            messageBoxText = "An error occurred. Check your internet connection and try again.";

            m.setText(messageBoxText);
            m.addButton("OK", QMessageBox::AcceptRole);
            m.exec();
        }
    }

    manualUpdateCheck = false;
}

bool KMI_Updates::slotReturnSkipUpdateBool(QString editorVersionFound)
{

    //qDebug() << "slotReturnSkipUpdateBool called";

    bool skipVersion = false;
    QString skipThisVersionString;

    if(sessionSettings->contains("softwareUpdateSkipVersion"))
    {
        skipThisVersionString = sessionSettings->value("softwareUpdateSkipVersion").toString();
    }
    else
    {
        sessionSettings->setValue("softwareUpdateSkipVersion", "0");
    }

    if(skipThisVersionString == editorVersionFound)
    {
        skipVersion = true;
    }

    qDebug() << "skip this version?" << skipVersion << "editorVersionFound: " << editorVersionFound << "skipVersion: " << skipThisVersionString;

    return skipVersion;
}

void KMI_Updates::slotGoToDownloadsPage()
{
    QDesktopServices::openUrl(QUrl(QString("http://www.keithmcmillen.com/downloads/#%1").arg(appName.toLower())));
}

void KMI_Updates::slotSkipVersion(QString versionToSkip)
{
    sessionSettings->setValue("softwareUpdateSkipVersion", versionToSkip);
}

