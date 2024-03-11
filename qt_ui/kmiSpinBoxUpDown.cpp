// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "kmiSpinBoxUpDown.h"

kmiSpinBoxUpDown::kmiSpinBoxUpDown(QWidget *parent) : QSpinBox(parent)
{
    sessionSettings = new QSettings(this);

    selectOnMousePress = false;

    QLineEdit *editor = this->findChild<QLineEdit *>("qt_spinbox_lineedit");
    editor->installEventFilter(this);
    this->installEventFilter(this);

#ifdef Q_OS_MAC
    QString spinBoxStyle = "QSpinBox{ font: 14pt \"Open Sans\"; background-color: rgba(1, 1, 1, 100); color: red;} QAbstractSpinBox:focus{ background-color: rgba(60, 60, 60, 100); color: white; outline: none;}";

#endif

#ifdef Q_OS_WINDOWS
    QString spinBoxStyle = "QSpinBox{ font: 10pt \"Open Sans\"; background-color: rgba(1, 1, 1, 100); color: red;} QAbstractSpinBox:focus{ background-color: rgba(60, 60, 60, 100); color: white; outline: none;}";
#endif

        this->setStyleSheet(spinBoxStyle);
    setupButtons();
}

void kmiSpinBoxUpDown::setupButtons() {
    // Hide the default up/down buttons
    setButtonSymbols(QAbstractSpinBox::NoButtons);

    // setup icons
    iconUpButton = ":/ui/images/arrow-up_pressed.svg";
    iconUpButtonPressed = ":/ui/images/arrow-up.svg";
    iconDownButton = ":/ui/images/arrow-down_pressed.svg";
    iconUDownButtonPressed = ":/ui/images/arrow-down.svg";

    // Create custom up and down buttons
    upButton = new QPushButton(this);
    downButton = new QPushButton(this);

    // Set icons for buttons
    upButton->setIcon(QIcon(iconUpButton));
    downButton->setIcon(QIcon(iconDownButton));

    // Change button icons on press and release
    connect(upButton, &QPushButton::pressed, this, [this]() { upButton->setIcon(QIcon(iconUpButtonPressed)); });
    connect(upButton, &QPushButton::released, this, [this]() { upButton->setIcon(QIcon(iconUpButton)); });

    connect(downButton, &QPushButton::pressed, this, [this]() { downButton->setIcon(QIcon(iconUDownButtonPressed)); });
    connect(downButton, &QPushButton::released, this, [this]() { downButton->setIcon(QIcon(iconDownButton)); });


    // Adjust button sizes and icons as needed
#define BUTTON_WIDTH 9
#define BUTTON_HEIGHT 9
#define BUTTON_PADDING_RIGHT 6
#define BUTTON_GAP  1
    QString buttonStyle = "border:none; background-color:rgba(0,0,0,0);";

    upButton->setIconSize(QSize(BUTTON_WIDTH, BUTTON_HEIGHT)); // Example size
    downButton->setIconSize(QSize(BUTTON_WIDTH, BUTTON_HEIGHT)); // Example size
    upButton->setStyleSheet(buttonStyle);
    downButton->setStyleSheet(buttonStyle);

    // Connect signals to slots for increasing and decreasing value
    connect(upButton, &QPushButton::clicked, this, [this]() { setValue(value() + singleStep()); });
    connect(downButton, &QPushButton::clicked, this, [this]() { setValue(value() - singleStep()); });

}


void kmiSpinBoxUpDown::focusInEvent(QFocusEvent *e)
{
    QSpinBox::focusInEvent(e);
    selectAll();
    selectOnMousePress = true;
    emit valueChanged(this->value());
}


bool kmiSpinBoxUpDown::eventFilter(QObject *obj, QEvent *event)
{
    bool toolTipsEnabled = sessionSettings->value("toolTipsEnabled").toBool();

    if(event->type() == QEvent::ToolTip)
    {
        if (!toolTipsEnabled)
        {
            //qDebug() << "spinbox tooltip suppresed";
            return true;
        }
        else
        {
            //qDebug() << "spinbox tooltip allowed";
            return QObject::eventFilter(obj, event);
        }
    }

    int stepMultiplier = 1; // Default step multiplier

    if(event->type() == QMouseEvent::MouseButtonPress)
    {
        //qDebug() << "mouse event";
        if(selectOnMousePress)
        {
            //qDebug() << "select all";
            this->selectAll();
            selectOnMousePress = false;
            return true;
        }
    }
    if (event->type() == QEvent::KeyPress)
    {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        //qDebug() << "kmiSpinBoxUpDown key press";



        if (keyEvent->modifiers() & Qt::ShiftModifier)
        {
            //qDebug() << "shift modifier";
            stepMultiplier = stepMultiplier * 10;
        }

        if (keyEvent->modifiers() & Qt::ControlModifier) {
            //qDebug() << "control modifier";
            // This block will execute for Control key on Windows, and command key on MacOS
            stepMultiplier = stepMultiplier * 10;
        }

        if (keyEvent->key() == Qt::Key_Up) {
            setValue(value() + singleStep() * stepMultiplier);
            return true; // Event handled
        } else if (keyEvent->key() == Qt::Key_Down) {
            setValue(value() - singleStep() * stepMultiplier);
            return true; // Event handled
        }
    }

    // Call base class eventFilter if not handled
    return QSpinBox::eventFilter(obj, event);
}

void kmiSpinBoxUpDown::resizeEvent(QResizeEvent *event) {
    Q_UNUSED(event);

    int xPosition = width() - BUTTON_WIDTH - BUTTON_PADDING_RIGHT; // x padding from the right edge
    int upButtonYPosition = (height() / 2) - BUTTON_HEIGHT - BUTTON_GAP; // Position for the up button
    int downButtonYPosition = (height() / 2) + BUTTON_GAP; // Directly below the up button

    upButton->move(xPosition, upButtonYPosition);
    downButton->move(xPosition, downButtonYPosition);
}

void kmiSpinBoxUpDown::changeFocus() {
    // Logic to remove focus; for example, set focus to the next widget
    QWidget *nextWidget = this->nextInFocusChain();
    if (nextWidget) {
        nextWidget->setFocus();
    }
}
