include(../common.pri)

TARGET = tst_seasideperson

PKGCONFIG += contactcache-qt5

SOURCES += $$SRCDIR/seasideperson.cpp \
           tst_seasideperson.cpp

HEADERS += $$SRCDIR/seasideperson.h
