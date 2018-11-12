AM_CFLAGS = -g -Wall -D_GNU_SOURCE

mthca_version_script = @MTHCA_VERSION_SCRIPT@

MTHCA_SOURCES = src/ah.c src/buf.c src/cq.c src/memfree.c src/mthca.c \
    src/qp.c src/srq.c src/verbs.c

if HAVE_IBV_DEVICE_LIBRARY_EXTENSION
    lib_LTLIBRARIES = src/libmthca.la
    src_libmthca_la_SOURCES = $(MTHCA_SOURCES)
    src_libmthca_la_LDFLAGS = -avoid-version -release @IBV_DEVICE_LIBRARY_EXTENSION@ \
        $(mthca_version_script)
    mthcaconfdir = $(sysconfdir)/libibverbs.d
    mthcaconf_DATA = mthca.driver
else
    mthcalibdir = $(libdir)/infiniband
    mthcalib_LTLIBRARIES = src/mthca.la
    src_mthca_la_SOURCES = $(MTHCA_SOURCES)
    src_mthca_la_LDFLAGS = -avoid-version -module $(mthca_version_script)
endif

DEBIAN = debian/changelog debian/compat debian/control debian/copyright \
    debian/libmthca1.install debian/libmthca-dev.install debian/rules

EXTRA_DIST = src/doorbell.h src/mthca.h src/mthca-abi.h src/wqe.h \
    src/mthca.map libmthca.spec.in mthca.driver

dist-hook: libmthca.spec
	cp libmthca.spec $(distdir)
