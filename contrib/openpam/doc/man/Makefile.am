# $Id: Makefile.am 594 2012-04-14 14:18:41Z des $

NULL =

# Standard PAM API
PMAN = \
	pam_acct_mgmt.3 \
	pam_authenticate.3 \
	pam_chauthtok.3 \
	pam_close_session.3 \
	pam_end.3 \
	pam_get_data.3 \
	pam_get_item.3 \
	pam_get_user.3 \
	pam_getenv.3 \
	pam_getenvlist.3 \
	pam_open_session.3 \
	pam_putenv.3 \
	pam_set_data.3 \
	pam_set_item.3 \
	pam_setcred.3 \
	pam_start.3 \
	pam_strerror.3 \
	$(NULL)

# Standard module API
MMAN = \
	pam_sm_acct_mgmt.3 \
	pam_sm_authenticate.3 \
	pam_sm_chauthtok.3 \
	pam_sm_close_session.3 \
	pam_sm_open_session.3 \
	pam_sm_setcred.3 \
	$(NULL)

# OpenPAM extensions
OMAN = \
	openpam_borrow_cred.3 \
	openpam_free_data.3 \
	openpam_free_envlist.3 \
	openpam_get_feature.3 \
	openpam_get_option.3 \
	openpam_log.3 \
	openpam_nullconv.3 \
	openpam_readline.3 \
	openpam_readlinev.3 \
	openpam_readword.3 \
	openpam_restore_cred.3 \
	openpam_set_feature.3 \
	openpam_set_option.3 \
	openpam_straddch.3 \
	openpam_subst.3 \
	openpam_ttyconv.3 \
	pam_error.3 \
	pam_get_authtok.3 \
	pam_info.3 \
	pam_prompt.3 \
	pam_setenv.3 \
	pam_verror.3 \
	pam_vinfo.3 \
	pam_vprompt.3 \
	$(NULL)

EXTRA_DIST = openpam.man pam.man

ALLCMAN = $(PMAN) $(MMAN) $(OMAN)

dist_man3_MANS = $(ALLCMAN) openpam.3 pam.3 pam_conv.3

dist_man5_MANS = pam.conf.5

CLEANFILES = $(ALLCMAN) openpam.3 pam.3

GENDOC = $(top_srcdir)/misc/gendoc.pl

LIBSRCDIR = $(top_srcdir)/lib

VPATH = $(LIBSRCDIR) $(srcdir)

SUFFIXES = .3

.c.3: $(GENDOC)
	perl -w $(GENDOC) $<

openpam.3: $(OMAN) $(GENDOC) $(srcdir)/openpam.man
	perl -w $(GENDOC) -o $(abs_srcdir)/$(OMAN) <$(srcdir)/openpam.man

pam.3: $(PMAN) $(GENDOC) $(srcdir)/pam.man
	perl -w $(GENDOC) -p $(abs_srcdir)/$(PMAN) <$(srcdir)/pam.man
