srcdir=../..
copyidsrcdir=..
prefix=/usr
exec_prefix=$(prefix)
bindir=$(prefix)/bin
datadir=$(prefix)/share
mandir=$(datadir)/man
docdir=$(datadir)/doc
sshdocdir=$(docdir)/openssh
cygdocdir=$(docdir)/Cygwin
sysconfdir=/etc
defaultsdir=$(sysconfdir)/defaults/etc
inetdefdir=$(defaultsdir)/inetd.d
PRIVSEP_PATH=/var/empty
INSTALL=/usr/bin/install -c
MKDIR_P=$(srcdir)/mkinstalldirs

DESTDIR=

all:
	@echo
	@echo "Use \`make cygwin-postinstall DESTDIR=[package directory]'"
	@echo "Be sure having DESTDIR set correctly!"
	@echo

move-config-files: $(DESTDIR)$(sysconfdir)/ssh_config $(DESTDIR)$(sysconfdir)/sshd_config
	$(MKDIR_P) $(DESTDIR)$(defaultsdir)
	mv $(DESTDIR)$(sysconfdir)/ssh_config $(DESTDIR)$(defaultsdir)
	mv $(DESTDIR)$(sysconfdir)/sshd_config $(DESTDIR)$(defaultsdir)

remove-empty-dir:
	rm -rf $(DESTDIR)$(PRIVSEP_PATH)

install-inetd-config:
	$(MKDIR_P) $(DESTDIR)$(inetdefdir)
	$(INSTALL) -m 644 sshd-inetd  $(DESTDIR)$(inetdefdir)/sshd-inetd

install-sshdoc:
	$(MKDIR_P) $(DESTDIR)$(sshdocdir)
	-$(INSTALL) -m 644 $(srcdir)/CREDITS $(DESTDIR)$(sshdocdir)/CREDITS
	-$(INSTALL) -m 644 $(srcdir)/ChangeLog $(DESTDIR)$(sshdocdir)/ChangeLog
	-$(INSTALL) -m 644 $(srcdir)/LICENCE $(DESTDIR)$(sshdocdir)/LICENCE
	-$(INSTALL) -m 644 $(srcdir)/OVERVIEW $(DESTDIR)$(sshdocdir)/OVERVIEW
	-$(INSTALL) -m 644 $(srcdir)/PROTOCOL $(DESTDIR)$(sshdocdir)/PROTOCOL
	-$(INSTALL) -m 644 $(srcdir)/PROTOCOL.agent $(DESTDIR)$(sshdocdir)/PROTOCOL.agent
	-$(INSTALL) -m 644 $(srcdir)/PROTOCOL.certkeys $(DESTDIR)$(sshdocdir)/PROTOCOL.certkeys
	-$(INSTALL) -m 644 $(srcdir)/PROTOCOL.mux $(DESTDIR)$(sshdocdir)/PROTOCOL.mux
	-$(INSTALL) -m 644 $(srcdir)/README $(DESTDIR)$(sshdocdir)/README
	-$(INSTALL) -m 644 $(srcdir)/README.dns $(DESTDIR)$(sshdocdir)/README.dns
	-$(INSTALL) -m 644 $(srcdir)/README.platform $(DESTDIR)$(sshdocdir)/README.platform
	-$(INSTALL) -m 644 $(srcdir)/README.privsep $(DESTDIR)$(sshdocdir)/README.privsep
	-$(INSTALL) -m 644 $(srcdir)/README.tun $(DESTDIR)$(sshdocdir)/README.tun
	-$(INSTALL) -m 644 $(srcdir)/TODO $(DESTDIR)$(sshdocdir)/TODO

install-cygwindoc: README
	$(MKDIR_P) $(DESTDIR)$(cygdocdir)
	$(INSTALL) -m 644 README $(DESTDIR)$(cygdocdir)/openssh.README

install-doc: install-sshdoc install-cygwindoc

install-scripts: ssh-host-config ssh-user-config
	$(MKDIR_P) $(DESTDIR)$(bindir)
	$(INSTALL) -m 755 ssh-host-config $(DESTDIR)$(bindir)/ssh-host-config
	$(INSTALL) -m 755 ssh-user-config $(DESTDIR)$(bindir)/ssh-user-config

install-copy-id: $(copyidsrcdir)/ssh-copy-id $(copyidsrcdir)/ssh-copy-id.1
	$(INSTALL) -m 755 $(copyidsrcdir)/ssh-copy-id $(DESTDIR)$(bindir)/ssh-copy-id
	$(INSTALL) -m 644 $(copyidsrcdir)/ssh-copy-id.1 $(DESTDIR)$(mandir)/man1/ssh-copy-id.1

gzip-man-pages:
	rm $(DESTDIR)$(mandir)/man1/slogin.1
	gzip $(DESTDIR)$(mandir)/man1/*.1
	gzip $(DESTDIR)$(mandir)/man5/*.5
	gzip $(DESTDIR)$(mandir)/man8/*.8
	cd $(DESTDIR)$(mandir)/man1 && ln -s ssh.1.gz slogin.1.gz

cygwin-postinstall: move-config-files remove-empty-dir install-inetd-config install-doc install-scripts install-copy-id gzip-man-pages
	@echo "Cygwin specific configuration finished."
