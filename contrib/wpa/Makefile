SUBDIRS=$(wildcard hostapd) $(wildcard wpa_supplicant)

all:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir; done

clean distclean:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir clean; done

install:
	for dir in $(SUBDIRS); do $(MAKE) -C $$dir install; done
