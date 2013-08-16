include(../common.pri)

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
