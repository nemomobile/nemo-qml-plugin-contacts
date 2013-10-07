include(../common.pri)

equals(QT_MAJOR_VERSION, 4) {
    PKGCONFIG += mlocale
}
equals(QT_MAJOR_VERSION, 5) {
    PKGCONFIG += mlocale5
}

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
