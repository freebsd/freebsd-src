# $FreeBSD$

# This Makefile is for use with the FreeBSD buid system.  For
# non-FreeBSD systems, you should first "make distfile" on FreeBSD,
# then use the resulting tar.gz archive--which contains a portable
# autoconf/automake-generated build system--on the other system.

LIB=	archive
DPADD=	${LIBBZ2} ${LIBZ}
LDADD=	-lbz2 -lz

# Versioning <sigh>.  There are three distinct sets of version
# numbers:
#    * libarchive version
#    * libtool version (for shared libs built using configure script)
#    * FreeBSD SHLIB_MAJOR
# The following logic helps ensure that there is some consistency across
# these three:

# First, the libarchive version is in three parts:
# Bumped ONLY when API/ABI breakage happens (non-backwards-compatible change).
ARCHIVE_API_MAJOR=	1
# Bumped when significant new features are added.
# Also used to bump SHLIB_MAJOR for reasons other than API/ABI breakage.
# Note: Do NOT reset this to zero after bumping ARCHIVE_API_MAJOR!
ARCHIVE_API_MINOR=	2
# Bumped often. ;-)
ARCHIVE_API_REVISION=	36
# Full libarchive version combines the above three numbers.
VERSION= ${ARCHIVE_API_MAJOR}.${ARCHIVE_API_MINOR}.${ARCHIVE_API_REVISION}

# The FreeBSD SHLIB_MAJOR is computed from the above values.
SHLIB_MAJOR!= echo $$((${ARCHIVE_API_MAJOR} + ${ARCHIVE_API_MINOR}))

# The SHLIB_MAJOR computation above helps ensure that the libtool
# version (computed in configure.ac.in) provides the same
# shared library number as is used on FreeBSD.

CFLAGS+=	-DPACKAGE_NAME=\"lib${LIB}\"
CFLAGS+=	-DPACKAGE_VERSION=\"${VERSION}\"
CFLAGS+=	-I${.OBJDIR}

# FreeBSD/arm has some limitations.
.if ${MACHINE_ARCH} == "arm"
WARNS?= 3
.else
WARNS?=	6
.endif

# Headers to be installed in /usr/include
INCS=	archive.h archive_entry.h

# C sources to be compiled.  This is one part of SRCS below.
BASE_SRCS= archive_check_magic.c			\
	archive_entry.c					\
	archive_read.c					\
	archive_read_data_into_buffer.c			\
	archive_read_data_into_fd.c			\
	archive_read_extract.c				\
	archive_read_open_fd.c				\
	archive_read_open_file.c			\
	archive_read_support_compression_all.c		\
	archive_read_support_compression_bzip2.c	\
	archive_read_support_compression_compress.c	\
	archive_read_support_compression_gzip.c		\
	archive_read_support_compression_none.c		\
	archive_read_support_format_all.c		\
	archive_read_support_format_cpio.c		\
	archive_read_support_format_iso9660.c		\
	archive_read_support_format_tar.c		\
	archive_read_support_format_zip.c		\
	archive_string.c				\
	archive_string_sprintf.c			\
	archive_util.c					\
	archive_write.c					\
	archive_write_open_fd.c				\
	archive_write_open_file.c			\
	archive_write_set_compression_bzip2.c		\
	archive_write_set_compression_gzip.c		\
	archive_write_set_compression_none.c		\
	archive_write_set_format.c			\
	archive_write_set_format_by_name.c		\
	archive_write_set_format_cpio.c			\
	archive_write_set_format_pax.c			\
	archive_write_set_format_shar.c			\
	archive_write_set_format_ustar.c

# Note: archive.h does need to be in SRCS, since it is built
# from archive.h.in.  But it does not get included in DIST_FILES
# below, so I build up SRCS in two steps.
SRCS=	${BASE_SRCS} archive.h

# Man pages to be installed.
MAN=	archive_entry.3					\
	archive_read.3					\
	archive_util.3					\
	archive_write.3					\
	libarchive.3					\
	libarchive-formats.5				\
	tar.5

# Symlink the man pages under each function name.
MLINKS+=	archive_entry.3 archive_entry_acl_add_entry.3
MLINKS+=	archive_entry.3 archive_entry_acl_add_entry_w.3
MLINKS+=	archive_entry.3 archive_entry_acl_clear.3
MLINKS+=	archive_entry.3 archive_entry_acl_count.3
MLINKS+=	archive_entry.3 archive_entry_acl_next.3
MLINKS+=	archive_entry.3 archive_entry_acl_next_w.3
MLINKS+=	archive_entry.3 archive_entry_acl_reset.3
MLINKS+=	archive_entry.3 archive_entry_acl_text_w.3
MLINKS+=	archive_entry.3 archive_entry_clear.3
MLINKS+=	archive_entry.3 archive_entry_clone.3
MLINKS+=	archive_entry.3 archive_entry_copy_fflags_text_w.3
MLINKS+=	archive_entry.3 archive_entry_copy_gname_w.3
MLINKS+=	archive_entry.3 archive_entry_copy_hardlink_w.3
MLINKS+=	archive_entry.3 archive_entry_copy_pathname_w.3
MLINKS+=	archive_entry.3 archive_entry_copy_stat.3
MLINKS+=	archive_entry.3 archive_entry_copy_symlink_w.3
MLINKS+=	archive_entry.3 archive_entry_copy_uname_w.3
MLINKS+=	archive_entry.3 archive_entry_fflags.3
MLINKS+=	archive_entry.3 archive_entry_fflags_text.3
MLINKS+=	archive_entry.3 archive_entry_free.3
MLINKS+=	archive_entry.3 archive_entry_gid.3
MLINKS+=	archive_entry.3 archive_entry_gname.3
MLINKS+=	archive_entry.3 archive_entry_gname_w.3
MLINKS+=	archive_entry.3 archive_entry_hardlink.3
MLINKS+=	archive_entry.3 archive_entry_ino.3
MLINKS+=	archive_entry.3 archive_entry_mode.3
MLINKS+=	archive_entry.3 archive_entry_mtime.3
MLINKS+=	archive_entry.3 archive_entry_mtime_nsec.3
MLINKS+=	archive_entry.3 archive_entry_new.3
MLINKS+=	archive_entry.3 archive_entry_pathname.3
MLINKS+=	archive_entry.3 archive_entry_pathname_w.3
MLINKS+=	archive_entry.3 archive_entry_rdev.3
MLINKS+=	archive_entry.3 archive_entry_rdevmajor.3
MLINKS+=	archive_entry.3 archive_entry_rdevminor.3
MLINKS+=	archive_entry.3 archive_entry_set_fflags.3
MLINKS+=	archive_entry.3 archive_entry_set_gid.3
MLINKS+=	archive_entry.3 archive_entry_set_gname.3
MLINKS+=	archive_entry.3 archive_entry_set_hardlink.3
MLINKS+=	archive_entry.3 archive_entry_set_link.3
MLINKS+=	archive_entry.3 archive_entry_set_mode.3
MLINKS+=	archive_entry.3 archive_entry_set_pathname.3
MLINKS+=	archive_entry.3 archive_entry_set_rdevmajor.3
MLINKS+=	archive_entry.3 archive_entry_set_rdevminor.3
MLINKS+=	archive_entry.3 archive_entry_set_size.3
MLINKS+=	archive_entry.3 archive_entry_set_symlink.3
MLINKS+=	archive_entry.3 archive_entry_set_uid.3
MLINKS+=	archive_entry.3 archive_entry_set_uname.3
MLINKS+=	archive_entry.3 archive_entry_size.3
MLINKS+=	archive_entry.3 archive_entry_stat.3
MLINKS+=	archive_entry.3 archive_entry_symlink.3
MLINKS+=	archive_entry.3 archive_entry_uid.3
MLINKS+=	archive_entry.3 archive_entry_uname.3
MLINKS+=	archive_entry.3 archive_entry_uname_w.3
MLINKS+=	archive_read.3 archive_read_data.3
MLINKS+=	archive_read.3 archive_read_data_block.3
MLINKS+=	archive_read.3 archive_read_data_into_buffer.3
MLINKS+=	archive_read.3 archive_read_data_into_fd.3
MLINKS+=	archive_read.3 archive_read_data_skip.3
MLINKS+=	archive_read.3 archive_read_extract.3
MLINKS+=	archive_read.3 archive_read_extract_set_progress_callback.3
MLINKS+=	archive_read.3 archive_read_finish.3
MLINKS+=	archive_read.3 archive_read_new.3
MLINKS+=	archive_read.3 archive_read_next_header.3
MLINKS+=	archive_read.3 archive_read_open.3
MLINKS+=	archive_read.3 archive_read_open_fd.3
MLINKS+=	archive_read.3 archive_read_open_file.3
MLINKS+=	archive_read.3 archive_read_set_bytes_per_block.3
MLINKS+=	archive_read.3 archive_read_support_compression_all.3
MLINKS+=	archive_read.3 archive_read_support_compression_bzip2.3
MLINKS+=	archive_read.3 archive_read_support_compression_compress.3
MLINKS+=	archive_read.3 archive_read_support_compression_gzip.3
MLINKS+=	archive_read.3 archive_read_support_compression_none.3
MLINKS+=	archive_read.3 archive_read_support_format_all.3
MLINKS+=	archive_read.3 archive_read_support_format_cpio.3
MLINKS+=	archive_read.3 archive_read_support_format_iso9660.3
MLINKS+=	archive_read.3 archive_read_support_format_tar.3
MLINKS+=	archive_read.3 archive_read_support_format_zip.3
MLINKS+=	archive_util.3 archive_compression.3
MLINKS+=	archive_util.3 archive_compression_name.3
MLINKS+=	archive_util.3 archive_errno.3
MLINKS+=	archive_util.3 archive_error_string.3
MLINKS+=	archive_util.3 archive_format.3
MLINKS+=	archive_util.3 archive_format_name.3
MLINKS+=	archive_util.3 archive_set_error.3
MLINKS+=	archive_write.3 archive_write_data.3
MLINKS+=	archive_write.3 archive_write_finish.3
MLINKS+=	archive_write.3 archive_write_header.3
MLINKS+=	archive_write.3 archive_write_new.3
MLINKS+=	archive_write.3 archive_write_open.3
MLINKS+=	archive_write.3 archive_write_open_fd.3
MLINKS+=	archive_write.3 archive_write_open_file.3
MLINKS+=	archive_write.3 archive_write_prepare.3
MLINKS+=	archive_write.3 archive_write_set_bytes_per_block.3
MLINKS+=	archive_write.3 archive_write_set_bytes_in_last_block.3
MLINKS+=	archive_write.3 archive_write_set_callbacks.3
MLINKS+=	archive_write.3 archive_write_set_compression_bzip2.3
MLINKS+=	archive_write.3 archive_write_set_compression_gzip.3
MLINKS+=	archive_write.3 archive_write_set_format_pax.3
MLINKS+=	archive_write.3 archive_write_set_format_shar.3
MLINKS+=	archive_write.3 archive_write_set_format_ustar.3
MLINKS+=	libarchive.3 archive.3

# Build archive.h from archive.h.in by substituting version information.
archive.h:	archive.h.in Makefile
	cat ${.CURDIR}/archive.h.in |					\
		sed 's/@VERSION@/${VERSION}/g' | 			\
		sed 's/@SHLIB_MAJOR@/${SHLIB_MAJOR}/g' |		\
		sed 's/@ARCHIVE_API_MAJOR@/${ARCHIVE_API_MAJOR}/g' |	\
		sed 's/@ARCHIVE_API_MINOR@/${ARCHIVE_API_MINOR}/g' |	\
		sed 's/@ARCHIVE_API_REVISION@/${ARCHIVE_API_REVISION}/g' | \
		cat > archive.h

# archive.h needs to be cleaned
CLEANFILES+=	archive.h

#
# Voodoo for building a distfile that uses autoconf/automake/etc.
#

# Files that just get copied to the distfile build directory
DIST_WORK_DIR=	${.OBJDIR}/lib${LIB}-${VERSION}
CLEANDIRS+=	${DIST_WORK_DIR}
DISTFILE=	lib${LIB}-${VERSION}.tar.gz
# DIST_FILES is the list of files to include in the distribution.
DIST_FILES=	${BASE_SRCS}
DIST_FILES+=	${MAN}
DIST_FILES+=	archive.h.in
DIST_FILES+=	archive_entry.h archive_platform.h
DIST_FILES+=	archive_private.h archive_string.h
DIST_FILES+=	Makefile.am

distfile:
	rm -rf ${DIST_WORK_DIR}
	mkdir ${DIST_WORK_DIR}
	# Copy the DIST_FILES; ignore errors, don't preserve permissions.
	for f in ${DIST_FILES}; do				\
		cat ${.CURDIR}/$$f >${DIST_WORK_DIR}/$$f || true; \
	done
	# Build configure.ac from configure.ac.in
	cat ${.CURDIR}/configure.ac.in |				\
		sed 's/@VERSION@/${VERSION}/g' |			\
		sed 's/@SHLIB_MAJOR@/${SHLIB_MAJOR}/g' | 		\
		sed 's/@ARCHIVE_API_MAJOR@/${ARCHIVE_API_MAJOR}/g' |	\
		sed 's/@ARCHIVE_API_MINOR@/${ARCHIVE_API_MINOR}/g' |	\
		sed 's/@ARCHIVE_API_REVISION@/${ARCHIVE_API_REVISION}/g' | \
		cat > ${DIST_WORK_DIR}/configure.ac
	# Prepare some auxiliary files.
	(cd ${DIST_WORK_DIR} && libtoolize)
	(cd ${DIST_WORK_DIR} && aclocal && autoheader)
	# Build the configure script and portable Makefile
	(cd ${DIST_WORK_DIR} && autoconf && automake -a --foreign)
	# Now, use automake-generated Makefile to build the final dist file.
	(cd ${DIST_WORK_DIR} && ./configure && make distcheck && make dist)
	mv ${DIST_WORK_DIR}/${DISTFILE} ${.OBJDIR}
	@echo ==================================================
	@echo Created ${.OBJDIR}/${DISTFILE}
	@echo ==================================================

.include <bsd.lib.mk>
