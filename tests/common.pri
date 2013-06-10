include(../src/src.pro)

SRCDIR = ../../src/
INCLUDEPATH += $$SRCDIR
DEPENDPATH = $$INCLUDEPATH

QT += testlib
TEMPLATE = app
CONFIG -= app_bundle

CONFIG += mobility
MOBILITY += contacts versit

equals(QT_MAJOR_VERSION, 4): target.path = /opt/tests/nemo-qml-plugins/contacts
equals(QT_MAJOR_VERSION, 5): target.path = /opt/tests/nemo-qml-plugins-qt5/contacts
INSTALLS += target
