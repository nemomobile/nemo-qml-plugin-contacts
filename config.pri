include(contacts-namespace.pri)

equals(QT_MAJOR_VERSION, 4) {
    CONFIG += mobility
    MOBILITY += contacts versit
}
equals(QT_MAJOR_VERSION, 5) {
    CONFIG += link_pkgconfig
    PKGCONFIG += Qt5Contacts Qt5Versit

    # Needed for qt4 moc, which can't handle numeric tests
    DEFINES *= QT_VERSION_5
}
