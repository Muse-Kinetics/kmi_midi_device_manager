// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef PEDALCAL_H
#define PEDALCAL_H

#include <QDialog>
#include <QVariant>
#include <QSettings>


namespace Ui {
class pedalCal;
}

class pedalCal : public QDialog
{
    Q_OBJECT



public:
    explicit pedalCal(QWidget *parent = nullptr);
    ~pedalCal();

    int inputVal, calMin, calMax, outputVal;

    QSettings *sessionSettings;

signals:
    void signalWindowClosed();
    void signalStoreValue(QString, QVariant);
    void signalSendCalibration();
    void signalSaveCalibration();

public slots:
    void slotConnectElements();
    void slotDisconnectElements();

    void slotSetDefaultValues();
    void slotProcessInput(int val);
    void slotSetInput(int val);
    void slotSetMin(int val);
    void slotSetMax(int val);
    void slotCalculateOutput();
    void slotLoadJSONCalibrationValues(QVariantMap preset);
    void slotSaveAndSendCalibrationValues();

protected:
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;


private:
    Ui::pedalCal *ui;
};

#endif // PEDALCAL_H
