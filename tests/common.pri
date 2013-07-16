include(../config.pri)
include(basename.pri)

TEMPLATE = app
CONFIG -= app_bundle

QT += testlib
equals(QT_MAJOR_VERSION, 4): QT += declarative
equals(QT_MAJOR_VERSION, 5): QT += qml

SRCDIR = $$PWD/../src/
INCLUDEPATH += $$SRCDIR
DEPENDPATH = $$INCLUDEPATH

target.path = /opt/tests/$${BASENAME}/contacts
INSTALLS += target
