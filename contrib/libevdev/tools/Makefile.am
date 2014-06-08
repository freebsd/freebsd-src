noinst_PROGRAMS = libevdev-events
bin_PROGRAMS = touchpad-edge-detector

AM_CPPFLAGS = $(GCC_CFLAGS) -I$(top_srcdir) -I$(top_srcdir)/include -I$(top_srcdir)/libevdev
libevdev_ldadd = $(top_builddir)/libevdev/libevdev.la

libevdev_events_SOURCES = libevdev-events.c
libevdev_events_LDADD = $(libevdev_ldadd)

touchpad_edge_detector_SOURCES = touchpad-edge-detector.c
touchpad_edge_detector_LDADD = $(libevdev_ldadd)

