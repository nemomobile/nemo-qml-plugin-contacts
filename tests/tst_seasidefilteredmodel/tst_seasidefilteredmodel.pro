include(../../config.pri)

QT += testlib
TEMPLATE = app
CONFIG -= app_bundle

SRCDIR = $$PWD/../../src

equals(QT_MAJOR_VERSION, 4): QT += declarative
equals(QT_MAJOR_VERSION, 5): QT += qml

equals(QT_MAJOR_VERSION, 4): target.path = /opt/tests/nemo-qml-plugins/contacts
equals(QT_MAJOR_VERSION, 5): target.path = /opt/tests/nemo-qml-plugins/contacts-qt5
INSTALLS += target

INCLUDEPATH += $$SRCDIR
HEADERS += \
        seasidecache.h \
        $$SRCDIR/seasidefilteredmodel.h \
        $$SRCDIR/seasideperson.h \
        $$SRCDIR/synchronizelists_p.h

SOURCES += \
        tst_seasidefilteredmodel.cpp \
        $$SRCDIR/seasidefilteredmodel.cpp \
        $$SRCDIR/seasideperson.cpp \
        seasidecache.cpp
