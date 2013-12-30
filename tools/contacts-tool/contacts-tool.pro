VERSION = 0.0.1
PROJECT_NAME = contacts-tool
TEMPLATE = app
CONFIG += ordered hide_symbols

include(../../config.pri) | error("Can't load config")

equals(QT_MAJOR_VERSION, 4) {
    CONFIG += mobility
    MOBILITY += contacts
    PKGCONFIG += contactcache
}
equals(QT_MAJOR_VERSION, 5) {
    # Needed for qt4 moc, which can't handle numeric tests
    DEFINES *= QT_VERSION_5

    CONFIG += link_pkgconfig
    PKGCONFIG += Qt5Contacts contactcache-qt5
}

QT += gui
TARGET = $$PROJECT_NAME
CONFIG -= app_bundle # OS X
DEFINES *= VERSION_STRING=\"\\\"$$VERSION\\\"\"

SOURCES += main.cpp

target.path = $$INSTALL_ROOT/usr/bin/
INSTALLS += target
