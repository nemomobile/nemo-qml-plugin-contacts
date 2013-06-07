TEMPLATE = subdirs
SUBDIRS = \
          tst_seasideperson \
          tst_seasidefilteredmodel \
          tst_synchronizelists

tests_xml.target = tests.xml
tests_xml.files = tests.xml
equals(QT_MAJOR_VERSION, 4): tests_xml.path = /opt/tests/nemo-qml-plugins/contacts
equals(QT_MAJOR_VERSION, 5): tests_xml.path = /opt/tests/nemo-qml-plugins/contacts-qt5
INSTALLS += tests_xml
