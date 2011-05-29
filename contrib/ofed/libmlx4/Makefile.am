AM_CFLAGS = -g -Wall -D_GNU_SOURCE

mlx4_version_script = @MLX4_VERSION_SCRIPT@

MLX4_SOURCES = src/buf.c src/cq.c src/dbrec.c src/mlx4.c src/qp.c \
    src/srq.c src/verbs.c

if HAVE_IBV_DEVICE_LIBRARY_EXTENSION
    lib_LTLIBRARIES = src/libmlx4.la
    src_libmlx4_la_SOURCES = $(MLX4_SOURCES)
    src_libmlx4_la_LDFLAGS = -avoid-version -release @IBV_DEVICE_LIBRARY_EXTENSION@ \
        $(mlx4_version_script)
    mlx4confdir = $(sysconfdir)/libibverbs.d
    mlx4conf_DATA = mlx4.driver
else
    mlx4libdir = $(libdir)/infiniband
    mlx4lib_LTLIBRARIES = src/mlx4.la
    src_mlx4_la_SOURCES = $(MLX4_SOURCES)
    src_mlx4_la_LDFLAGS = -avoid-version -module $(mlx4_version_script)
endif

EXTRA_DIST = src/doorbell.h src/mlx4.h src/mlx4-abi.h src/wqe.h \
    src/mlx4.map libmlx4.spec.in mlx4.driver

dist-hook: libmlx4.spec
	cp libmlx4.spec $(distdir)
