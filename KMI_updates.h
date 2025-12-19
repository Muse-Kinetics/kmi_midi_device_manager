// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef KMI_UPDATES_H
#define KMI_UPDATES_H

#include <QtWidgets>
#include <QNetworkAccessManager>
#include <QNetworkReply>

class KMI_Updates : public QWidget
{
    Q_OBJECT

public:
    explicit KMI_Updates(QWidget *p = 0, QString an = "", QSettings *sessionSettings = 0, QByteArray av = 0, QString initURL = "");

    QWidget *parent;
    QSettings *sessionSettings;
    QNetworkAccessManager *networkAccessManager;
    QByteArray applicationVersion;
    QString appName;

    QString jsonVersionURL;

    bool manualUpdateCheck;

public slots:

    void slotManualCheckForUpdates();
    void slotCheckForUpdates();
    void slotUpdateCheckReply(QNetworkReply *networkReply);
    bool slotReturnSkipUpdateBool(QString editorVersionFound);
    void slotGoToDownloadsPage();
    void slotSkipVersion(QString versionToSkip);
};

#endif // KMI_UPDATES_H
