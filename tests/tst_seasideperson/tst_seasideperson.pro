include(../common.pri)

TARGET = tst_seasideperson

equals(QT_MAJOR_VERSION, 4): PKGCONFIG += contactcache
equals(QT_MAJOR_VERSION, 5): PKGCONFIG += contactcache-qt5

SOURCES += $$SRCDIR/seasideperson.cpp \
           tst_seasideperson.cpp

HEADERS += $$SRCDIR/seasideperson.h
