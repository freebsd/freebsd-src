QT += quick widgets gui qml
CONFIG += c++11

TARGET = papagan-dock
TEMPLATE = app

SOURCES += main.cpp

RESOURCES +=

QML_IMPORT_PATH += qml

# Install into ${PREFIX}/bin by ports/Makefile
target.path = $$[QT_INSTALL_BINS]
INSTALLS += target
