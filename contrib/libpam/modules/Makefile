# $Id: Makefile,v 1.21 1997/04/05 06:44:43 morgan Exp morgan $
#
# Makefile
#
# This makefile controls the build process of shared and static PAM modules.
#
# $Log: Makefile,v $
# Revision 1.21  1997/04/05 06:44:43  morgan
# pam_env and pam_tally added
#
# Revision 1.20  1997/02/15 18:57:11  morgan
# fixed bash syntax
#
# Revision 1.19  1997/01/04 20:21:32  morgan
# moved responsibility of conditional compilation to modules (more flexible)
#
# Revision 1.18  1996/12/01 03:34:40  morgan
# update for .54
#
# Revision 1.17  1996/11/10 20:20:15  morgan
# cross platform support and new modules
#
# Revision 1.16  1996/09/05 06:20:45  morgan
# added two modules: listfile and shells
#
# Revision 1.15  1996/08/09 05:38:28  morgan
# added new/proposed modules.
# fixed makefile installation dependencies
#
# Revision 1.14  1996/07/08 00:00:33  morgan
# added wheel and group modules
#

MODDIRS=\
	pam_access \
	pam_afs \
	pam_afsauth \
	pam_afspass \
	pam_afstok \
	pam_cracklib \
	pam_deny \
	pam_desgold \
	pam_env \
	pam_filter \
	pam_ftp \
	pam_group \
	pam_kerberos \
	pam_krb4 \
	pam_lastlog \
	pam_listfile \
	pam_limits \
	pam_mail \
	pam_nologin \
	pam_opie \
	pam_passwd+ \
	pam_permit \
	pam_pwdb \
	pam_radius \
	pam_restrict \
	pam_rhosts \
	pam_rootok \
	pam_securetty \
	pam_shells \
	pam_sid \
	pam_skey \
	pam_skey2 \
	pam_stress \
	pam_syslog \
	pam_tally \
	pam_time \
	pam_unix \
	pam_warn \
	pam_wheel


# ////////////////////////////////////////////////////
# // You should not modify anything below this line //
# ////////////////////////////////////////////////////

dummy:
	@echo	"*** This is not a top-level Makefile! ***"

# -----------------------------------------------------------

all:	
	@echo modules for $(OS) are:
	@ls -d $(MODDIRS) 2>/dev/null ; echo :--------
	@echo
ifdef STATIC
	rm -f ./_static_module_*
endif
	@for i in $(MODDIRS) ; do \
		if [ -d $$i ]; then { \
		   $(MAKE) -C $$i all ; \
		   if [ $$? -ne 0 ]; then exit 1 ; fi ; \
		} elif [ -f ./.$$i ]; then { \
		   cat ./.$$i ; \
		} fi ; \
	done

install:
	for i in $(MODDIRS) ; do \
		if [ -d $$i ]; then { \
		$(MAKE) -C $$i install ; \
		if [ $$? -ne 0 ]; then exit 1 ; fi ; \
		} fi ; \
	done

remove:
	for i in $(MODDIRS) ; do \
		if [ -d $$i ]; then { \
			$(MAKE) -C $$i remove ; \
		} fi ; \
	done

lclean:
	rm -f _static_module_*

clean: lclean
	for i in $(MODDIRS) ; do \
		if [ -d $$i ]; then { \
			$(MAKE) -C $$i clean ; \
		} fi ; \
	done 

extraclean: lclean
	for i in $(MODDIRS) ; do \
		if [ -d $$i ]; then \
			$(MAKE) -C $$i extraclean ; \
		fi ; \
	done

