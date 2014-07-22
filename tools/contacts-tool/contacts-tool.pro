PROJECT_NAME = contacts-tool
TEMPLATE = app
CONFIG += ordered hide_symbols

include(../../config.pri) | error("Can't load config")

CONFIG += link_pkgconfig
PKGCONFIG += Qt5Contacts contactcache-qt5

QT += gui
TARGET = $$PROJECT_NAME
CONFIG -= app_bundle # OS X
DEFINES *= VERSION_STRING=\"\\\"$$VERSION\\\"\"

SOURCES += main.cpp

target.path = $$INSTALL_ROOT/usr/bin/
INSTALLS += target
