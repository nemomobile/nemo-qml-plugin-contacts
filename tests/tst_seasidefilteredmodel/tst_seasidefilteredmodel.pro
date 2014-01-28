include(../common.pri)

PKGCONFIG += mlocale5

CONFIG += c++11

HEADERS += \
        seasidecache.h \
        seasidefilteredmodel.h \
        $$SRCDIR/seasidefilteredmodel.h \
        $$SRCDIR/seasideperson.h

SOURCES += \
        seasidecache.cpp \
        tst_seasidefilteredmodel.cpp \
        $$SRCDIR/seasidefilteredmodel.cpp \
        $$SRCDIR/seasideperson.cpp
