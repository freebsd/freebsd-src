# $P4: //depot/projects/openpam/lib/Makefile.am#5 $

NULL =

INCLUDES = -I$(top_srcdir)/include

lib_LTLIBRARIES = libpam.la

libpam_la_SOURCES = \
	openpam_borrow_cred.c \
	openpam_configure.c \
	openpam_dispatch.c \
	openpam_dynamic.c \
	openpam_findenv.c \
	openpam_free_data.c \
	openpam_free_envlist.c \
	openpam_get_option.c \
	openpam_load.c \
	openpam_log.c \
	openpam_nullconv.c \
	openpam_readline.c \
	openpam_restore_cred.c \
	openpam_set_option.c \
	openpam_static.c \
	openpam_ttyconv.c \
	pam_acct_mgmt.c \
	pam_authenticate.c \
	pam_chauthtok.c \
	pam_close_session.c \
	pam_end.c \
	pam_error.c \
	pam_get_authtok.c \
	pam_get_data.c \
	pam_get_item.c \
	pam_get_user.c \
	pam_getenv.c \
	pam_getenvlist.c \
	pam_info.c \
	pam_open_session.c \
	pam_prompt.c \
	pam_putenv.c \
	pam_set_data.c \
	pam_set_item.c \
	pam_setcred.c \
	pam_setenv.c \
	pam_start.c \
	pam_strerror.c \
	pam_verror.c \
	pam_vinfo.c \
	pam_vprompt.c \
	$(NULL)

libpam_la_CFLAGS = -DOPENPAM_MODULES_DIR='"@OPENPAM_MODULES_DIR@/"'

libpam_la_LDFLAGS = -no-undefined -version-info @LIB_MAJ@ @DL_LIBS@

# Not implemented yet:
#	pam_authenticate_secondary.c
#	pam_get_mapped_authtok.c
#	pam_get_mapped_username.c
#	pam_set_mapped_authtok.c
#	pam_set_mapped_username.c
