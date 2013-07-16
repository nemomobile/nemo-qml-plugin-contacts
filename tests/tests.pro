include(basename.pri)

TEMPLATE = subdirs
SUBDIRS = \
          tst_seasideperson \
          tst_seasidefilteredmodel

tests_xml.target = tests.xml
tests_xml.depends = $$PWD/tests.xml.in
tests_xml.commands = sed -e "s:@BASENAME@:$${BASENAME}:g" $< > $@
tests_xml.CONFIG += no_check_exist

QMAKE_EXTRA_TARGETS = tests_xml
QMAKE_CLEAN += $$tests_xml.target
PRE_TARGETDEPS += $$tests_xml.target

tests_install.path = /opt/tests/$${BASENAME}/contacts/
tests_install.files = $$tests_xml.target
tests_install.depends = tests_xml
tests_install.CONFIG += no_check_exist

INSTALLS += tests_install
