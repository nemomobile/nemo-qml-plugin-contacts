TARGET = nemocontacts
PLUGIN_IMPORT_PATH = org/nemomobile/contacts

TEMPLATE = lib
CONFIG += qt plugin hide_symbols
QT += declarative

target.path = $$[QT_INSTALL_IMPORTS]/$$PLUGIN_IMPORT_PATH
INSTALLS += target

qmldir.files += $$_PRO_FILE_PWD_/qmldir
qmldir.path +=  $$[QT_INSTALL_IMPORTS]/$$$$PLUGIN_IMPORT_PATH
INSTALLS += qmldir


CONFIG += link_pkgconfig
packagesExist(mlite) {
    PKGCONFIG += mlite
    DEFINES += HAS_MLITE
} else {
    warning("mlite not available. Some functionality may not work as expected.")
}

CONFIG += mobility
MOBILITY += contacts versit

SOURCES += $$PWD/plugin.cpp \
           $$PWD/normalization_p.cpp \
           $$PWD/seasidepeoplemodel.cpp \
           $$PWD/seasidepeoplemodel_p.cpp \
           $$PWD/seasideperson.cpp \
           $$PWD/seasideproxymodel.cpp \
           $$PWD/seasidecache.cpp \
           $$PWD/seasidefilteredmodel.cpp \
           $$PWD/seasidenamegroupmodel.cpp

HEADERS += \
           $$PWD/normalization_p.h \
           $$PWD/synchronizelists_p.h \
           $$PWD/seasidepeoplemodel.h \
           $$PWD/seasidepeoplemodel_p.h \
           $$PWD/seasideperson.h \
           $$PWD/seasideproxymodel.h \
           $$PWD/seasidecache.h \
           $$PWD/seasidefilteredmodel.h \
           $$PWD/seasidenamegroupmodel.h

MOC_DIR = $$PWD/../.moc
OBJECTS_DIR = $$PWD/../.obj
