QT       += core gui charts serialport serialbus multimedia

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    aboutdialog.cpp \
    audiosettingsdialog.cpp \
    generator.cpp \
    ledindicator.cpp \
    livechartwidget.cpp \
    main.cpp \
    mainwindow.cpp \
    modbusconfigdialog.cpp \
    modbusreader.cpp

HEADERS += \
    aboutdialog.h \
    audiosettingsdialog.h \
    generator.h \
    ledindicator.h \
    livechartwidget.h \
    mainwindow.h \
    modbusconfigdialog.h \
    modbusreader.h

FORMS += \
    aboutdialog.ui \
    audiosettingsdialog.ui \
    mainwindow.ui \
    modbusconfigdialog.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    resource.qrc

RC_FILE = app.rc
