QT       += core gui charts serialport serialbus

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    aboutdialog.cpp \
    ledindicator.cpp \
    livechartwidget.cpp \
    main.cpp \
    mainwindow.cpp \
    modbusconfigdialog.cpp \
    modbusreader.cpp

HEADERS += \
    aboutdialog.h \
    ledindicator.h \
    livechartwidget.h \
    mainwindow.h \
    modbusconfigdialog.h \
    modbusreader.h \
    skewed_lorentzian_fit.hpp

FORMS += \
    aboutdialog.ui \
    mainwindow.ui \
    modbusconfigdialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource.qrc

RC_FILE = app.rc


# QXlsx code for Application Qt project
QXLSX_PARENTPATH=./QXlsx/         # current QXlsx path is . (. means curret directory)
QXLSX_HEADERPATH=./QXlsx/header/  # current QXlsx header path is ./header/
QXLSX_SOURCEPATH=./QXlsx/source/  # current QXlsx source path is ./source/
include(./QXlsx/QXlsx.pri)
