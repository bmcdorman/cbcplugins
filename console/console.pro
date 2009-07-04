# -------------------------------------------------
# Project created by QtCreator 2009-07-04T10:00:42
# -------------------------------------------------
QT -= gui
TARGET = cbc
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app
SOURCES += SerialClient.cpp \
    QSerialPort.cpp \
    CBC.cpp \
    main.cpp
OTHER_FILES += README
HEADERS += QSerialPort.h \
    SerialClient.h \
    CBC.h
