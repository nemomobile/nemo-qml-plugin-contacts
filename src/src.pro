include(../config.pri)

TARGET = nemocontacts
PLUGIN_IMPORT_PATH = org/nemomobile/contacts

TEMPLATE = lib
CONFIG += qt plugin hide_symbols

equals(QT_MAJOR_VERSION, 4): QT += declarative
equals(QT_MAJOR_VERSION, 5): QT += qml

equals(QT_MAJOR_VERSION, 4): target.path = $$[QT_INSTALL_IMPORTS]/$$PLUGIN_IMPORT_PATH
equals(QT_MAJOR_VERSION, 5): target.path = $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += $$_PRO_FILE_PWD_/qmldir
equals(QT_MAJOR_VERSION, 4): qmldir.path +=  $$[QT_INSTALL_IMPORTS]/$$PLUGIN_IMPORT_PATH
equals(QT_MAJOR_VERSION, 5): qmldir.path +=  $$[QT_INSTALL_QML]/$$PLUGIN_IMPORT_PATH
INSTALLS += qmldir

CONFIG += link_pkgconfig
equals(QT_MAJOR_VERSION, 4) {
    packagesExist(mlite) {
        PKGCONFIG += mlite
        DEFINES += HAS_MLITE
    } else {
        warning("mlite not available. Some functionality may not work as expected.")
    }
}
equals(QT_MAJOR_VERSION, 5) {
    packagesExist(mlite5) {
        PKGCONFIG += mlite5
        DEFINES += HAS_MLITE
    } else {
        warning("mlite not available. Some functionality may not work as expected.")
    }
}

SOURCES += $$PWD/plugin.cpp \
           $$PWD/normalization_p.cpp \
           $$PWD/seasideperson.cpp \
           $$PWD/seasidecache.cpp \
           $$PWD/seasidefilteredmodel.cpp \
           $$PWD/seasidenamegroupmodel.cpp

HEADERS += \
           $$PWD/normalization_p.h \
           $$PWD/synchronizelists_p.h \
           $$PWD/seasideperson.h \
           $$PWD/seasidecache.h \
           $$PWD/seasidefilteredmodel.h \
           $$PWD/seasidenamegroupmodel.h

MOC_DIR = $$PWD/../.moc
OBJECTS_DIR = $$PWD/../.obj
