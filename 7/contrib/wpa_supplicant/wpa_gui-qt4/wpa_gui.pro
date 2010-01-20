TEMPLATE	= app
LANGUAGE	= C++

CONFIG	+= qt warn_on release

DEFINES += CONFIG_CTRL_IFACE

win32 {
  LIBS += -lws2_32 -static
  DEFINES += CONFIG_NATIVE_WINDOWS CONFIG_CTRL_IFACE_NAMED_PIPE
} else:win32-g++ {
  # cross compilation to win32
  LIBS += -lws2_32 -static
  DEFINES += CONFIG_NATIVE_WINDOWS CONFIG_CTRL_IFACE_NAMED_PIPE
} else {
  DEFINES += CONFIG_CTRL_IFACE_UNIX
}

INCLUDEPATH	+= . .. ../../hostapd

HEADERS	+= wpamsg.h \
	wpagui.h \
	eventhistory.h \
	scanresults.h \
	userdatarequest.h \
	networkconfig.h

SOURCES	+= main.cpp \
	wpagui.cpp \
	eventhistory.cpp \
	scanresults.cpp \
	userdatarequest.cpp \
	networkconfig.cpp \
	../wpa_ctrl.c

FORMS	= wpagui.ui \
	eventhistory.ui \
	scanresults.ui \
	userdatarequest.ui \
	networkconfig.ui


unix {
  UI_DIR = .ui
  MOC_DIR = .moc
  OBJECTS_DIR = .obj
}

# TODO: remove need for Qt3 support code
QT += qt3support
