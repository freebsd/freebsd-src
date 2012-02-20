#!/bin/bash
rm -f /usr/bin/csh /usr/bin/csh.exe /usr/bin/csh.lnk /usr/share/man/man1/csh.1*
cd /bin
ln -s tcsh csh
cd /usr/share/man/man1
ln -s tcsh.1 csh.1
[ -e /etc/csh.cshrc ] || cp /etc/defaults/etc/csh.cshrc /etc
[ -e /etc/csh.login ] || cp /etc/defaults/etc/csh.login /etc
[ -e /etc/profile.d/bindkey.tcsh ] || \
	cp /etc/defaults/etc/profile.d/bindkey.tcsh /etc/profile.d
[ -e /etc/profile.d/complete.tcsh ] || \
	cp /etc/defaults/etc/profile.d/complete.tcsh /etc/profile.d
