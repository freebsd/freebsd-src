#
# $Id: Makefile,v 1.1.1.1 2000/06/20 22:10:44 agmorgan Exp $
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

lclean:
	rm -f core *~ .ignore_age

clean: lclean
	$(MAKE) -C pam_conv1 clean

extraclean: lclean
	$(MAKE) -C pam_conv1 extraclean
