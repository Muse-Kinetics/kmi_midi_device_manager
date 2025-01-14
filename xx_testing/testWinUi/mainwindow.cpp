// Copyright (c) 2025 KMI Music, Inc.
// This Source Code Form is subject to the terms of the Mozilla Public License, v. 2.0.
// If a copy of the MPL was not distributed with this file, You can obtain one at https://mozilla.org/MPL/2.0/.
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDialog>
#include <QVBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QFont>
#include <QDebug>
#include <QStyle>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    cvCal *cvCalWindow = new cvCal();



    QPalette palette;

    // Set palette colors based on your stylesheet
    palette.setColor(QPalette::Window, QColor(0, 0, 0, 90)); // For QWidget background
    palette.setColor(QPalette::ButtonText, Qt::white); // For QPushButton text color
    palette.setColor(QPalette::Button, QColor(60, 60, 60)); // For QPushButton background
    palette.setColor(QPalette::WindowText, QColor(242, 242, 242)); // For QLabel text color

    // Apply the palette to your widget or application
    cvCalWindow->setPalette(palette);


    //QString thisStyle = "QWidget {background-color: rgba(160, 160, 160, 50);}";

    QFile *cvStyleFile;
    cvStyleFile = new QFile(":/cvCalStyleWin.qss");
    if (!cvStyleFile->open(QFile::ReadOnly))
    {
        qDebug() << "Error opening file";
    }
    QString thisStyle = QLatin1String(cvStyleFile->readAll());


    qDebug() << "------------------------------------------------------";
    // Print the style sheet of cvCalWindow
    qDebug() << "cvCal Window StyleSheet:" << cvCalWindow->styleSheet();

    // Print the palette color for the Window background role
    qDebug() << "cvCal Window Palette Background Color:"
             << cvCalWindow->palette().color(QPalette::Window);

    // Print the palette color for the Window text role
    qDebug() << "cvCal Window Palette Text Color:"
             << cvCalWindow->palette().color(QPalette::WindowText);

    // Print the effective stylesheet (which is the concatenation of the parent widget's stylesheet)
    qDebug() << "cvCal Window Effective StyleSheet:"
             << cvCalWindow->styleSheet();

    // Print the widget's font
    qDebug() << "cvCal Window Font:"
             << cvCalWindow->font().toString();

    // Print the widget's inherited palette (this includes inherited palette from the parent, if any)
    qDebug() << "cvCal Window Inherited Palette Background Color:"
             << cvCalWindow->palette().color(cvCalWindow->backgroundRole());

    qDebug() << "------------------------------------------------------";

    cvCalWindow->setStyleSheet(thisStyle);

    // Print the style sheet of cvCalWindow
    qDebug() << "cvCal Window StyleSheet:" << cvCalWindow->styleSheet();

    // Print the palette color for the Window background role
    qDebug() << "cvCal Window Palette Background Color:"
             << cvCalWindow->palette().color(QPalette::Window);

    // Print the palette color for the Window text role
    qDebug() << "cvCal Window Palette Text Color:"
             << cvCalWindow->palette().color(QPalette::WindowText);

    // Print the effective stylesheet (which is the concatenation of the parent widget's stylesheet)
    qDebug() << "cvCal Window Effective StyleSheet:"
             << cvCalWindow->styleSheet();

    // Print the widget's font
    qDebug() << "cvCal Window Font:"
             << cvCalWindow->font().toString();

    // Print the widget's inherited palette (this includes inherited palette from the parent, if any)
    qDebug() << "cvCal Window Inherited Palette Background Color:"
             << cvCalWindow->palette().color(cvCalWindow->backgroundRole());

    cvCalWindow->show();
    cvCalWindow->raise();

    cvCalWindow->style()->unpolish(cvCalWindow);
    cvCalWindow->style()->polish(cvCalWindow);
    cvCalWindow->update();


}

//    // Create the dialog
//    QDialog *thisDialog = new QDialog(this); // 'this' makes MainWindow the parent
//    thisDialog->setWindowTitle("Styled Dialog");
//    thisDialog->setStyleSheet("background: black; color: white;"); // Simple stylesheet for example

//    QVBoxLayout *layout = new QVBoxLayout(thisDialog); // Use the dialog as the layout's parent

//    // Add some content to the dialog
//    QLabel *label = new QLabel("This dialog is shown at startup.");
//    QPushButton *button = new QPushButton("Close");

//    layout->addWidget(label);
//    layout->addWidget(button);

//    // Connect the button to close the dialog
//    connect(button, &QPushButton::clicked, thisDialog, &QDialog::accept);

//    // Show the dialog
//    thisDialog->setLayout(layout);
//    thisDialog->setAttribute(Qt::WA_DeleteOnClose); // Ensure the dialog is deleted once closed
//    thisDialog->exec(); // Use exec() for a modal dialog, or show() for modeless
//}


MainWindow::~MainWindow()
{
    delete ui;
}




