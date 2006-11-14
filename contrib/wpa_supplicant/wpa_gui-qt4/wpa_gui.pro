TEMPLATE	= app
LANGUAGE	= C++

CONFIG	+= qt warn_on release

win32 {
  LIBS += -lws2_32 -static
  DEFINES += CONFIG_NATIVE_WINDOWS CONFIG_CTRL_IFACE_UDP
} else:win32-g++ {
  # cross compilation to win32
  LIBS += -lws2_32 -static
  DEFINES += CONFIG_NATIVE_WINDOWS CONFIG_CTRL_IFACE_UDP
}

INCLUDEPATH	+= . ..

HEADERS	+= wpamsg.h

SOURCES	+= main.cpp \
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



#The following line was inserted by qt3to4
QT += qt3support 
#The following line was inserted by qt3to4
CONFIG += uic3

