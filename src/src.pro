include(../config.pri)

TARGET = nemocontacts
PLUGIN_IMPORT_PATH = org/nemomobile/contacts

TEMPLATE = lib
CONFIG += qt plugin hide_symbols

QT += qml
PKGCONFIG += contactcache-qt5 mlocale5

packagesExist(mlite5) {
    PKGCONFIG += mlite5
    DEFINES += HAS_MLITE
} else {
    warning("mlite not available. Some functionality may not work as expected.")
}

target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += $$_PRO_FILE_PWD_/qmldir
qmldir.path +=  $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += qmldir

SOURCES += $$PWD/plugin.cpp \
           $$PWD/seasideperson.cpp \
           $$PWD/seasidefilteredmodel.cpp \
           $$PWD/seasidenamegroupmodel.cpp \
           $$PWD/seasidevcardmodel.cpp

HEADERS += $$PWD/seasideperson.h \
           $$PWD/seasidefilteredmodel.h \
           $$PWD/seasidenamegroupmodel.h \
           $$PWD/seasidevcardmodel.h
