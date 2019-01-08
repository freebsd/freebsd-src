
# note that order matters: make the libs first then use them
SUBDIRS = complib libvendor opensm osmtest include $(DEFAULT_EVENT_PLUGIN)
DIST_SUBDIRS = complib libvendor opensm osmtest include osmeventplugin

ACLOCAL_AMFLAGS = -I config

# we should provide a hint for other apps about the build mode of this project
install-exec-hook:
	mkdir -p $(DESTDIR)/$(includedir)
if DEBUG
	echo "define osm_build_type \"debug\"" > $(DESTDIR)/$(includedir)/infiniband/opensm/osm_build_id.h
else
	echo "define osm_build_type \"free\"" > $(DESTDIR)/$(includedir)/infiniband/opensm/osm_build_id.h
endif
	$(top_srcdir)/config/install-sh -m 755 -d $(DESTDIR)/$(sysconfdir)/init.d
	cp $(top_builddir)/scripts/opensm.init $(DESTDIR)/$(sysconfdir)/init.d/opensmd
	chmod 755 $(DESTDIR)/$(sysconfdir)/init.d/opensmd


man_MANS = man/opensm.8 man/osmtest.8

various_scripts = $(wildcard scripts/*)
docs = doc/performance-manager-HOWTO.txt doc/QoS_management_in_OpenSM.txt \
	doc/opensm_release_notes-3.2.txt

EXTRA_DIST = autogen.sh opensm.spec $(various_scripts) $(man_MANS) $(docs)

dist-hook: $(EXTRA_DIST)
	if [ -x $(top_srcdir)/../gen_chlog.sh ] ; then \
		cd $(top_srcdir)/.. ; ./gen_chlog.sh $(PACKAGE) > $(distdir)/ChangeLog ; cd - ; \
	fi
