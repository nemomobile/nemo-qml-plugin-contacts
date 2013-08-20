include(../config.pri)

TARGET = nemocontacts
PLUGIN_IMPORT_PATH = org/nemomobile/contacts

TEMPLATE = lib
CONFIG += qt plugin hide_symbols

equals(QT_MAJOR_VERSION, 4) {
    QT += declarative
    PKGCONFIG += contactcache
}
equals(QT_MAJOR_VERSION, 5) {
    QT += qml
    PKGCONFIG += contactcache-qt5
}

equals(QT_MAJOR_VERSION, 4): target.path = $$[QT_INSTALL_IMPORTS]/$$PLUGIN_IMPORT_PATH
equals(QT_MAJOR_VERSION, 5): target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += $$_PRO_FILE_PWD_/qmldir
equals(QT_MAJOR_VERSION, 4): qmldir.path +=  $$[QT_INSTALL_IMPORTS]/$$PLUGIN_IMPORT_PATH
equals(QT_MAJOR_VERSION, 5): qmldir.path +=  $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += qmldir

SOURCES += $$PWD/plugin.cpp \
           $$PWD/seasideperson.cpp \
           $$PWD/seasidefilteredmodel.cpp \
           $$PWD/seasidenamegroupmodel.cpp

HEADERS += $$PWD/seasideperson.h \
           $$PWD/seasidefilteredmodel.h \
           $$PWD/seasidenamegroupmodel.h
