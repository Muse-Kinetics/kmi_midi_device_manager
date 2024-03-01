// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#ifndef KMISPINBOXUPDOWN_H
#define KMISPINBOXUPDOWN_H

#include <QSpinBox>
#include <QPushButton>
#include <QHBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QLineEdit>
#include <QSettings>

class kmiSpinBoxUpDown : public QSpinBox {
    Q_OBJECT

public:
    explicit kmiSpinBoxUpDown(QWidget *parent = nullptr);

    QSettings *sessionSettings;

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    //void keyPressEvent(QKeyEvent *e) override;
    void focusInEvent(QFocusEvent *e) override;

    bool selectOnMousePress;

private:
    QPushButton *upButton;
    QPushButton *downButton;
    void setupButtons();
    void changeFocus();

    QString iconUpButton, iconUpButtonPressed, iconDownButton, iconUDownButtonPressed;
};

#endif // KMISPINBOXUPDOWN_H
