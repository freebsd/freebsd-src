srcdir=../..
prefix=/usr
exec_prefix=$(prefix)
bindir=$(prefix)/bin
datadir=$(prefix)/share
docdir=$(datadir)/doc
sshdocdir=$(docdir)/openssh
cygdocdir=$(docdir)/Cygwin
sysconfdir=/etc
defaultsdir=$(sysconfdir)/defaults/etc
PRIVSEP_PATH=/var/empty
INSTALL=/usr/bin/install -c

DESTDIR=

all:
	@echo
	@echo "Use \`make cygwin-postinstall DESTDIR=[package directory]'"
	@echo "Be sure having DESTDIR set correctly!"
	@echo

move-config-files: $(DESTDIR)$(sysconfdir)/ssh_config $(DESTDIR)$(sysconfdir)/sshd_config
	$(srcdir)/mkinstalldirs $(DESTDIR)$(defaultsdir)
	mv $(DESTDIR)$(sysconfdir)/ssh_config $(DESTDIR)$(defaultsdir)
	mv $(DESTDIR)$(sysconfdir)/sshd_config $(DESTDIR)$(defaultsdir)

remove-empty-dir:
	rm -rf $(DESTDIR)$(PRIVSEP_PATH)

install-sshdoc:
	$(srcdir)/mkinstalldirs $(DESTDIR)$(sshdocdir)
	$(INSTALL) -m 644 $(srcdir)/CREDITS $(DESTDIR)$(sshdocdir)/CREDITS
	$(INSTALL) -m 644 $(srcdir)/ChangeLog $(DESTDIR)$(sshdocdir)/ChangeLog
	$(INSTALL) -m 644 $(srcdir)/LICENCE $(DESTDIR)$(sshdocdir)/LICENCE
	$(INSTALL) -m 644 $(srcdir)/OVERVIEW $(DESTDIR)$(sshdocdir)/OVERVIEW
	$(INSTALL) -m 644 $(srcdir)/README $(DESTDIR)$(sshdocdir)/README
	$(INSTALL) -m 644 $(srcdir)/README.dns $(DESTDIR)$(sshdocdir)/README.dns
	$(INSTALL) -m 644 $(srcdir)/README.privsep $(DESTDIR)$(sshdocdir)/README.privsep
	$(INSTALL) -m 644 $(srcdir)/README.smartcard $(DESTDIR)$(sshdocdir)/README.smartcard
	$(INSTALL) -m 644 $(srcdir)/RFC.nroff $(DESTDIR)$(sshdocdir)/RFC.nroff
	$(INSTALL) -m 644 $(srcdir)/TODO $(DESTDIR)$(sshdocdir)/TODO
	$(INSTALL) -m 644 $(srcdir)/WARNING.RNG $(DESTDIR)$(sshdocdir)/WARNING.RNG

install-cygwindoc: README
	$(srcdir)/mkinstalldirs $(DESTDIR)$(cygdocdir)
	$(INSTALL) -m 644 README $(DESTDIR)$(cygdocdir)/openssh.README

install-doc: install-sshdoc install-cygwindoc

install-scripts: ssh-host-config ssh-user-config
	$(srcdir)/mkinstalldirs $(DESTDIR)$(bindir)
	$(INSTALL) -m 755 ssh-host-config $(DESTDIR)$(bindir)/ssh-host-config
	$(INSTALL) -m 755 ssh-user-config $(DESTDIR)$(bindir)/ssh-user-config

cygwin-postinstall: move-config-files remove-empty-dir install-doc install-scripts
	@echo "Cygwin specific configuration finished."
