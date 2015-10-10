#-------------------------------------------------
#
# Project created by QtCreator 2015-10-08T20:45:25
#
#-------------------------------------------------

QT       += core network

QT       -= gui

TARGET = z-http-service
CONFIG   += console c++11
CONFIG   -= app_bundle

TEMPLATE = app


SOURCES += main.cpp \
    zhttpserver.cpp

HEADERS += \
    zhttpserver.h

#DESTDIR         = $$_PRO_FILE_PWD_/
OTHER_FILES += z-http.service

service.path = /etc/systemd/system/
service.files = z-http.service

target.path = /usr/bin/

INSTALLS += target service
