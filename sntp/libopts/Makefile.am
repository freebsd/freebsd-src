## LIBOPTS Makefile
MAINTAINERCLEANFILES    = Makefile.in
if INSTALL_LIBOPTS
lib_LTLIBRARIES         = libopts.la
else
noinst_LTLIBRARIES      = libopts.la
endif
libopts_la_SOURCES      = libopts.c
libopts_la_CPPFLAGS     = -I$(top_srcdir)
libopts_la_LDFLAGS      = -version-info  35:0:10
EXTRA_DIST              = \
    COPYING.gplv3           COPYING.lgplv3          COPYING.mbsd  \
    MakeDefs.inc            README                  ag-char-map.h  \
    autoopts/usage-txt.h    autoopts/options.h      autoopts/project.h  \
    autoopts.c              autoopts.h              boolean.c  \
    compat/strdup.c         compat/compat.h         compat/windows-config.h  \
    compat/snprintf.c       compat/strchr.c         compat/pathfind.c  \
    configfile.c            cook.c                  enumeration.c  \
    environment.c           file.c                  genshell.c  \
    genshell.h              load.c                  m4/liboptschk.m4  \
    m4/libopts.m4           makeshell.c             nested.c  \
    numeric.c               parse-duration.c        parse-duration.h  \
    pgusage.c               proto.h                 putshell.c  \
    reset.c                 restore.c               save.c  \
    sort.c                  stack.c                 streqvcmp.c  \
    text_mmap.c             time.c                  tokenize.c  \
    usage.c                 value-type.c            value-type.h  \
    version.c               xat-attribute.c         xat-attribute.h
