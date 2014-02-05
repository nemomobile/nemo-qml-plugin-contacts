include(../config.pri)
include(basename.pri)

TEMPLATE = app
CONFIG -= app_bundle

QT += testlib qml

SRCDIR = $$PWD/../src/
INCLUDEPATH += $$SRCDIR
DEPENDPATH = $$INCLUDEPATH

target.path = /opt/tests/$${BASENAME}/contacts
INSTALLS += target
