Summary: OpenSSH, a free Secure Shell (SSH) protocol implementation
Name: openssh
Version: 3.8.1p1
URL: http://www.openssh.com/
Release: 1
Source0: openssh-%{version}.tar.gz
Copyright: BSD
Group: Applications/Internet
BuildRoot: /tmp/openssh-%{version}-buildroot
PreReq: openssl
Obsoletes: ssh
#
# (Build[ing] Prereq[uisites] only work for RPM 2.95 and newer.)
# building prerequisites -- stuff for
#   OpenSSL (openssl-devel),
#   TCP Wrappers (nkitb),
#   and Gnome (glibdev, gtkdev, and gnlibsd)
#
BuildPrereq: openssl
BuildPrereq: nkitb
BuildPrereq: glibdev
BuildPrereq: gtkdev
BuildPrereq: gnlibsd

%description
Ssh (Secure Shell) a program for logging into a remote machine and for
executing commands in a remote machine.  It is intended to replace
rlogin and rsh, and provide secure encrypted communications between
two untrusted hosts over an insecure network.  X11 connections and
arbitrary TCP/IP ports can also be forwarded over the secure channel.

OpenSSH is OpenBSD's rework of the last free version of SSH, bringing it
up to date in terms of security and features, as well as removing all
patented algorithms to seperate libraries (OpenSSL).

This package includes all files necessary for both the OpenSSH
client and server. Additionally, this package contains the GNOME
passphrase dialog.

%changelog
* Mon Jun 12 2000 Damien Miller <djm@mindrot.org>
- Glob manpages to catch compressed files
* Wed Mar 15 2000 Damien Miller <djm@ibs.com.au>
- Updated for new location
- Updated for new gnome-ssh-askpass build
* Sun Dec 26 1999 Chris Saia <csaia@wtower.com>
- Made symlink to gnome-ssh-askpass called ssh-askpass
* Wed Nov 24 1999 Chris Saia <csaia@wtower.com>
- Removed patches that included /etc/pam.d/sshd, /sbin/init.d/rc.sshd, and
  /var/adm/fillup-templates/rc.config.sshd, since Damien merged these into
  his released tarfile
- Changed permissions on ssh_config in the install procedure to 644 from 600
  even though it was correct in the %files section and thus right in the RPMs
- Postinstall script for the server now only prints "Generating SSH host
  key..." if we need to actually do this, in order to eliminate a confusing
  message if an SSH host key is already in place
- Marked all manual pages as %doc(umentation)
* Mon Nov 22 1999 Chris Saia <csaia@wtower.com>
- Added flag to configure daemon with TCP Wrappers support
- Added building prerequisites (works in RPM 3.0 and newer)
* Thu Nov 18 1999 Chris Saia <csaia@wtower.com>
- Made this package correct for SuSE.
- Changed instances of pam_pwdb.so to pam_unix.so, since it works more properly
  with SuSE, and lib_pwdb.so isn't installed by default.
* Mon Nov 15 1999 Damien Miller <djm@mindrot.org>
- Split subpackages further based on patch from jim knoble <jmknoble@pobox.com>
* Sat Nov 13 1999 Damien Miller <djm@mindrot.org>
- Added 'Obsoletes' directives
* Tue Nov 09 1999 Damien Miller <djm@ibs.com.au>
- Use make install
- Subpackages
* Mon Nov 08 1999 Damien Miller <djm@ibs.com.au>
- Added links for slogin
- Fixed perms on manpages
* Sat Oct 30 1999 Damien Miller <djm@ibs.com.au>
- Renamed init script
* Fri Oct 29 1999 Damien Miller <djm@ibs.com.au>
- Back to old binary names
* Thu Oct 28 1999 Damien Miller <djm@ibs.com.au>
- Use autoconf
- New binary names
* Wed Oct 27 1999 Damien Miller <djm@ibs.com.au>
- Initial RPMification, based on Jan "Yenya" Kasprzak's <kas@fi.muni.cz> spec.

%prep

%setup -q

%build
CFLAGS="$RPM_OPT_FLAGS" \
./configure	--prefix=/usr \
		--sysconfdir=/etc/ssh \
		--datadir=/usr/share/openssh \
		--with-pam \
		--with-gnome-askpass \
		--with-tcp-wrappers \
		--with-ipv4-default \
		--libexecdir=/usr/lib/ssh
make

cd contrib
gcc -O -g `gnome-config --cflags gnome gnomeui` \
	gnome-ssh-askpass.c -o gnome-ssh-askpass \
	`gnome-config --libs gnome gnomeui`
cd ..

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT/
install -d $RPM_BUILD_ROOT/etc/ssh/
install -d $RPM_BUILD_ROOT/etc/pam.d/
install -d $RPM_BUILD_ROOT/sbin/init.d/
install -d $RPM_BUILD_ROOT/var/adm/fillup-templates
install -d $RPM_BUILD_ROOT/usr/lib/ssh
install -m644 contrib/sshd.pam.generic $RPM_BUILD_ROOT/etc/pam.d/sshd
install -m744 contrib/suse/rc.sshd $RPM_BUILD_ROOT/sbin/init.d/sshd
ln -s ../../sbin/init.d/sshd $RPM_BUILD_ROOT/usr/sbin/rcsshd
install -s contrib/gnome-ssh-askpass $RPM_BUILD_ROOT/usr/lib/ssh/gnome-ssh-askpass
ln -s gnome-ssh-askpass $RPM_BUILD_ROOT/usr/lib/ssh/ssh-askpass
install -m744 contrib/suse/rc.config.sshd \
   $RPM_BUILD_ROOT/var/adm/fillup-templates

%clean
rm -rf $RPM_BUILD_ROOT

%post
if [ "$1" = 1 ]; then
  echo "Creating SSH stop/start scripts in the rc directories..."
  ln -s ../sshd /sbin/init.d/rc2.d/K20sshd
  ln -s ../sshd /sbin/init.d/rc2.d/S20sshd
  ln -s ../sshd /sbin/init.d/rc3.d/K20sshd
  ln -s ../sshd /sbin/init.d/rc3.d/S20sshd
fi
echo "Updating /etc/rc.config..."
if [ -x /bin/fillup ] ; then
  /bin/fillup -q -d = etc/rc.config var/adm/fillup-templates/rc.config.sshd
else
  echo "ERROR: fillup not found.  This should NOT happen in SuSE Linux."
  echo "Update /etc/rc.config by hand from the following template file:"
  echo "  /var/adm/fillup-templates/rc.config.sshd"
fi
if [ ! -f /etc/ssh/ssh_host_key -o ! -s /etc/ssh/ssh_host_key ]; then
	echo "Generating SSH host key..."
	/usr/bin/ssh-keygen -b 1024 -f /etc/ssh/ssh_host_key -N '' >&2
fi
if [ ! -f /etc/ssh/ssh_host_dsa_key -o ! -s /etc/ssh/ssh_host_dsa_key ]; then
	echo "Generating SSH DSA host key..."
	/usr/bin/ssh-keygen -d -f /etc/ssh/ssh_host_dsa_key -N '' >&2
fi
if test -r /var/run/sshd.pid
then
	echo "Restarting the running SSH daemon..."
	/usr/sbin/rcsshd restart >&2
fi

%preun
if [ "$1" = 0 ]
then
	echo "Stopping the SSH daemon..."
	/usr/sbin/rcsshd stop >&2
	echo "Removing SSH stop/start scripts from the rc directories..."
	rm /sbin/init.d/rc2.d/K20sshd
	rm /sbin/init.d/rc2.d/S20sshd
	rm /sbin/init.d/rc3.d/K20sshd
	rm /sbin/init.d/rc3.d/S20sshd
fi

%files
%defattr(-,root,root)
%doc ChangeLog OVERVIEW README*
%doc RFC.nroff TODO CREDITS LICENCE
%attr(0755,root,root) %dir /etc/ssh
%attr(0644,root,root) %config /etc/ssh/ssh_config
%attr(0600,root,root) %config /etc/ssh/sshd_config
%attr(0600,root,root) %config /etc/ssh/moduli
%attr(0644,root,root) %config /etc/pam.d/sshd
%attr(0755,root,root) %config /sbin/init.d/sshd
%attr(0755,root,root) /usr/bin/ssh-keygen
%attr(0755,root,root) /usr/bin/scp
%attr(4755,root,root) /usr/bin/ssh
%attr(-,root,root) /usr/bin/slogin
%attr(0755,root,root) /usr/bin/ssh-agent
%attr(0755,root,root) /usr/bin/ssh-add
%attr(0755,root,root) /usr/bin/ssh-keyscan
%attr(0755,root,root) /usr/bin/sftp
%attr(0755,root,root) /usr/sbin/sshd
%attr(-,root,root) /usr/sbin/rcsshd
%attr(0755,root,root) %dir /usr/lib/ssh
%attr(0755,root,root) /usr/lib/ssh/ssh-askpass
%attr(0755,root,root) /usr/lib/ssh/gnome-ssh-askpass
%attr(0644,root,root) %doc /usr/man/man1/scp.1*
%attr(0644,root,root) %doc /usr/man/man1/ssh.1*
%attr(-,root,root) %doc /usr/man/man1/slogin.1*
%attr(0644,root,root) %doc /usr/man/man1/ssh-agent.1*
%attr(0644,root,root) %doc /usr/man/man1/ssh-add.1*
%attr(0644,root,root) %doc /usr/man/man1/ssh-keygen.1*
%attr(0644,root,root) %doc /usr/man/man8/sshd.8*
%attr(0644,root,root) /var/adm/fillup-templates/rc.config.sshd

