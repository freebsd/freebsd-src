#
# $Id: Makefile,v 1.8 1997/04/05 06:59:33 morgan Exp $
#
# $Log: Makefile,v $
# Revision 1.8  1997/04/05 06:59:33  morgan
# fakeroot and $(MAKE)
#
# Revision 1.7  1997/02/15 15:53:51  morgan
# added lines to make pam_conv1
#
# Revision 1.6  1996/11/10 19:48:09  morgan
# fix for systems that have not installed bash in /bin/
#
# Revision 1.5  1996/03/16 22:21:26  morgan
# added 'make remove' option
#
# Revision 1.4  1996/03/10 21:01:47  morgan
# added .ignore_age flag file
#
# Revision 1.3  1996/03/10 17:41:28  morgan
# make RCScheck check for the presence of the executable before running
# it!
#
# Revision 1.2  1996/03/10 17:16:42  morgan
# added md5RCS/ RCScheck entry
#
#

dummy:
	@echo "*** This is not a top level Makefile!"

##########################################################

all:
	$(MAKE) -C pam_conv1 all

install: $(FAKEROOT)$(CONFIGED)/pam.conf
	$(MAKE) -C pam_conv1 install

$(FAKEROOT)$(CONFIGED)/pam.conf: ./pam.conf
	bash -f ./install_conf

remove:
	rm -f $(FAKEROOT)$(CONFIGED)/pam.conf
	$(MAKE) -C pam_conv1 remove

check:
	bash -f ./md5itall

RCScheck:
	if [ -x ./md5RCS ]; then bash -f ./md5RCS ; fi

lclean:
	rm -f core *~ .ignore_age

clean: lclean
	$(MAKE) -C pam_conv1 clean

extraclean: lclean
	$(MAKE) -C pam_conv1 extraclean
