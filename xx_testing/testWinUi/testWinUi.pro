QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ../../cvCal/cvCal.cpp \
    ../../qt_ui/kmiSpinBoxUpDown.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    ../../../../presetimageformatting/12step.h \
    ../../../../presetimageformatting/DeviceManager/sysex.h \
    ../../../../presetimageformatting/DeviceManager/sysexcmds.h \
    ../../cvCal/cvCal.h \
    ../../cvCal/cvCalData.h \
    ../../midi.h \
    ../../qt_ui/kmiSpinBoxUpDown.h \
    mainwindow.h

FORMS += \
    ../../cvCal/cvCal.ui \
    mainwindow.ui

INCLUDEPATH += \
    ../../cvCal \
    ../../../../presetimageformatting/ \
    ../../../../presetimageformatting/DeviceManager \
    ../../ \
    ../../qt_ui/ \

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    ../../cvCal/cvCalStyleMac.qss \
    ../../cvCal/cvCalStyleWin.qss

RESOURCES += \
    resources.qrc
