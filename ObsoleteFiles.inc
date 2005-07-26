#
# $FreeBSD$
#
# This file lists old files (OLD_FILES), libraries (OLD_LIBS) and
# directories (OLD_DIRS) which should get removed at an update. Recently
# removed entries first (with the date as a comment). Dynamic libraries are
# special cased (OLD_LIBS). Static libraries or the generic links to
# the dynamic libraries (lib*.so) should (if you don't know why to make an
# exception, make this a "must") be viewed as normal files (OLD_FILES).
#
# In case of a complete directory hierarchy the sorting is in depth first
# order.
#

# 20050721: moved to ports
OLD_FILES+=usr/sbin/vttest
# 20050610: rexecd (insecure by design)
OLD_FILES+=etc/pam.d/rexecd
OLD_FILES+=usr/share/man/man8/rexecd.8.gz
OLD_FILES+=usr/libexec/rexecd
# 20050324: updated release infrastructure
OLD_FILES+=usr/share/man/man5/drivers.conf.5.gz
# 20050317: removed from BIND 9 distribution
OLD_FILES+=usr/share/doc/bind9/KNOWN_DEFECTS
# 2005XXXX:
OLD_FILES+=sbin/mount_autofs
OLD_FILES+=usr/lib/libautofs.a
OLD_FILES+=usr/lib/libautofs.so
# 20050203 Merged with fortunes
OLD_FILES+=usr/share/games/fortune/fortunes2
OLD_FILES+=usr/share/games/fortune/fortunes2.dat
# 20041109: replaced by em(4)
OLD_FILES+=usr/share/man/man4/gx.4.gz
OLD_FILES+=usr/share/man/man4/if_gx.4.gz
# 20040925: bind9 import
OLD_FILES+=usr/bin/dnskeygen
OLD_FILES+=usr/bin/dnsquery
OLD_FILES+=usr/lib/libisc.a
OLD_FILEs+=usr/lib/libisc.so
OLD_FILES+=usr/lib/libisc_p.a
OLD_FILES+=usr/libexec/named-xfer
OLD_FILES+=usr/sbin/named.restart
OLD_FILES+=usr/sbin/ndc
OLD_FILES+=usr/sbin/nslookup
OLD_FILES+=usr/sbin/nsupdate
OLD_DIRS+=usr/share/doc/bind
# 2004XXYY: OS internal libs, no ports use them, no need to use OLD_LIBS
OLD_FILES+=lib/geom/geom_concat.so.1
OLD_FILES+=lib/geom/geom_label.so.1
OLD_FILES+=lib/geom/geom_nop.so.1
OLD_FILES+=lib/geom/geom_stripe.so.1
.if ${MACHINE_ARCH} != "alpha" && ${MACHINE_ARCH} != "sparc64"
# 20040130: libkse renamed to libpthread
OLD_FILES+=usr/lib/libkse.a
OLD_FILES+=usr/lib/libkse.so
.endif
# 200XYYZZ: /lib transition clitches
OLD_FILES+=lib/libalias.so
OLD_FILES+=lib/libatm.so
OLD_FILES+=lib/libbsdxml.so
OLD_FILES+=lib/libc.so
OLD_FILES+=lib/libcam.so
OLD_FILES+=lib/libcrypt.so
OLD_FILES+=lib/libcrypto.so
OLD_FILES+=lib/libdevstat.so
OLD_FILES+=lib/libedit.so
OLD_FILES+=lib/libgeom.so
OLD_FILES+=lib/libipsec.so
OLD_FILES+=lib/libipx.so
OLD_FILES+=lib/libkvm.so
OLD_FILES+=lib/libm.so
OLD_FILES+=lib/libmd.so
OLD_FILES+=lib/libncurses.so
OLD_FILES+=lib/libreadline.so
OLD_FILES+=lib/libsbuf.so
OLD_FILES+=lib/libufs.so
OLD_FILES+=lib/libz.so
# 200304XX
OLD_FILES+=usr/lib/libcipher.a
OLD_FILES+=usr/lib/libcipher.so
OLD_FILES+=usr/lib/libcipher_p.a
OLD_FILES+=usr/lib/libgmp.a
OLD_FILES+=usr/lib/libgmp.so
OLD_FILES+=usr/lib/libgmp_p.a
OLD_FILES+=usr/lib/libperl.a
OLD_FILES+=usr/lib/libperl.so
OLD_FILES+=usr/lib/libperl_p.a
OLD_FILES+=usr/lib/libposix1e.a
OLD_FILES+=usr/lib/libposix1e.so
OLD_FILES+=usr/lib/libposix1e_p.a
OLD_FILES+=usr/lib/libskey.a
OLD_FILES+=usr/lib/libskey.so
OLD_FILES+=usr/lib/libskey_p.a
# 200303XX
OLD_FILES+=usr/lib/libacl.so
OLD_FILES+=usr/lib/libdescrypt.so
OLD_FILES+=usr/lib/libf2c.so
OLD_FILES+=usr/lib/libg++.so
OLD_FILES+=usr/lib/libkdb.so
OLD_FILES+=usr/lib/librsaINTL.so
OLD_FILES+=usr/lib/libscrypt.so
OLD_FILES+=usr/lib/libss.so
# 200302XX
OLD_FILES+=usr/lib/libacl.a
OLD_FILES+=usr/lib/libacl_p.a
OLD_FILES+=usr/lib/libkadm.a
OLD_FILES+=usr/lib/libkadm.so
OLD_FILES+=usr/lib/libkadm_p.a
OLD_FILES+=usr/lib/libkafs.a
OLD_FILES+=usr/lib/libkafs.so
OLD_FILES+=usr/lib/libkafs_p.a
OLD_FILES+=usr/lib/libkdb.a
OLD_FILES+=usr/lib/libkdb_p.a
OLD_FILES+=usr/lib/libkrb.a
OLD_FILES+=usr/lib/libkrb.so
OLD_FILES+=usr/lib/libkrb_p.a
# 200206XX
OLD_FILES+=usr/lib/libpam_ssh.a
OLD_FILES+=usr/lib/libpam_ssh_p.a
# 200203XX
OLD_FILES+=usr/lib/libss.a
OLD_FILES+=usr/lib/libss_p.a
OLD_FILES+=usr/lib/libtelnet.a
OLD_FILES+=usr/lib/libtelnet_p.a
OLD_FILES+=usr/lib/libusb.a
OLD_FILES+=usr/lib/libusb.so
OLD_FILES+=usr/lib/libusb_p.a
# 2001XXXX
OLD_FILES+=etc/security
# 200104XX
OLD_FILES+=usr/lib/libdescrypt.a
OLD_FILES+=usr/lib/libscrypt.a
OLD_FILES+=usr/lib/libscrypt_p.a
# 20001200
OLD_FILES+=usr/lib/libgcc_r_pic.a
# 200009XX
OLD_FILES+=usr/lib/libRSAglue.a
OLD_FILES+=usr/lib/libRSAglue.so
OLD_FILES+=usr/lib/librsaINTL.a
OLD_FILES+=usr/lib/librsaUSA.a
OLD_FILES+=usr/lib/librsaUSA.so
# 200002XX ?
OLD_FILES+=usr/lib/libf2c.a
OLD_FILES+=usr/lib/libf2c_p.a
OLD_FILES+=usr/lib/libg++.a
OLD_FILES+=usr/lib/libg++_p.a
# 20001006
OLD_FILES+=usr/bin/miniperl
# 20000810
OLD_FILES+=usr/bin/sperl*
# 199909XX
OLD_FILES+=usr/lib/libdesrypt_p.a
## unsorted
# do we still support aout builds?
#OLD_FILES+=usr/lib/aout/c++rt0.o
#OLD_FILES+=usr/lib/aout/crt0.o
#OLD_FILES+=usr/lib/aout/gcrt0.o
#OLD_FILES+=usr/lib/aout/scrt0.o
#OLD_FILES+=usr/lib/aout/sgcrt0.o

# 20050722: bump for 6.0-RELEASE
OLD_LIBS+=lib/libalias.so.4
OLD_LIBS+=lib/libatm.so.2
OLD_LIBS+=lib/libbegemot.so.1
OLD_LIBS+=lib/libbsdxml.so.1
OLD_LIBS+=lib/libbsnmp.so.2
OLD_LIBS+=lib/libc.so.5
OLD_LIBS+=lib/libcam.so.2
OLD_LIBS+=lib/libcrypt.so.2
OLD_LIBS+=lib/libcrypto.so.3
OLD_LIBS+=lib/libdevstat.so.4
OLD_LIBS+=lib/libedit.so.4
OLD_LIBS+=lib/libgeom.so.2
OLD_LIBS+=lib/libgpib.so.0
OLD_LIBS+=lib/libipsec.so.1
OLD_LIBS+=lib/libipx.so.2
OLD_LIBS+=lib/libkiconv.so.1
OLD_LIBS+=lib/libkvm.so.2
OLD_LIBS+=lib/libm.so.3
OLD_LIBS+=lib/libmd.so.2
OLD_LIBS+=lib/libncurses.so.5
OLD_LIBS+=lib/libreadline.so.5
OLD_LIBS+=lib/libsbuf.so.2
OLD_LIBS+=lib/libufs.so.2
OLD_LIBS+=lib/libutil.so.4
OLD_LIBS+=lib/libz.so.2
OLD_LIBS+=usr/lib/libarchive.so.1
OLD_LIBS+=usr/lib/libasn1.so.7
OLD_LIBS+=usr/lib/libbluetooth.so.1
OLD_LIBS+=usr/lib/libbz2.so.1
OLD_LIBS+=usr/lib/libc_r.so.5
OLD_LIBS+=usr/lib/libcalendar.so.2
OLD_LIBS+=usr/lib/libcom_err.so.2
OLD_LIBS+=usr/lib/libdevinfo.so.2
OLD_LIBS+=usr/lib/libdialog.so.4
OLD_LIBS+=usr/lib/libfetch.so.3
OLD_LIBS+=usr/lib/libform.so.2
OLD_LIBS+=usr/lib/libftpio.so.5
OLD_LIBS+=usr/lib/libg2c.so.1
OLD_LIBS+=usr/lib/libgnuregex.so.2
OLD_LIBS+=usr/lib/libgssapi.so.7
OLD_LIBS+=usr/lib/libhdb.so.7
OLD_LIBS+=usr/lib/libhistory.so.5
OLD_LIBS+=usr/lib/libkadm5clnt.so.7
OLD_LIBS+=usr/lib/libkadm5srv.so.7
OLD_LIBS+=usr/lib/libafs5.so.7
OLD_LIBS+=usr/lib/libkrb5.so.7
OLD_LIBS+=usr/lib/libmagic.so.1
OLD_LIBS+=usr/lib/libmenu.so.2
OLD_LIBS+=usr/lib/libmilter.so.2
OLD_LIBS+=usr/lib/libmp.so.4
OLD_LIBS+=usr/lib/libncp.so.1
OLD_LIBS+=usr/lib/libnetgraph.so.1
OLD_LIBS+=usr/lib/libngatm.so.1
OLD_LIBS+=usr/lib/libobjc.so.1
OLD_LIBS+=usr/lib/libopie.so.3
OLD_LIBS+=usr/lib/libpam.so.2
OLD_LIBS+=usr/lib/libpanel.so.2
OLD_LIBS+=usr/lib/libpcap.so.3
OLD_LIBS+=usr/lib/libpmc.so.2
OLD_LIBS+=usr/lib/libpthread.so.1
OLD_LIBS+=usr/lib/libradius.so.1
OLD_LIBS+=usr/lib/libroken.so.7
OLD_LIBS+=usr/lib/librpcsvc.so.2
OLD_LIBS+=usr/lib/libsdp.so.1
OLD_LIBS+=usr/lib/libsmb.so.1
OLD_LIBS+=usr/lib/libssh.so.2
OLD_LIBS+=usr/lib/libssl.so.3
OLD_LIBS+=usr/lib/libstdc++.so.4
OLD_LIBS+=usr/lib/libtacplus.so.1
OLD_LIBS+=usr/lib/libthr.so.1
OLD_LIBS+=usr/lib/libthread_db.so.1
OLD_LIBS+=usr/lib/libugidfw.so.1
OLD_LIBS+=usr/lib/libusbhid.so.1
OLD_LIBS+=usr/lib/libvgl.so.3
OLD_LIBS+=usr/lib/libwrap.so.3
OLD_LIBS+=usr/lib/libypclnt.so.1
OLD_LIBS+=usr/lib/pam_*.so.2
OLD_LIBS+=usr/lib/snmp_*.so.3
# 20050603: network related ABI change on 64bit systems
OLD_LIBS+=usr/lib/libpcap.so.3
# 200505XX: ?
OLD_LIBS+=usr/lib/snmp_*.so.2
# 2005XXXX: not ready for primetime yet
OLD_LIBS+=usr/lib/libautofs.so.1
# 200410XX: libm compatibility fix
OLD_LIBS+=lib/libm.so.2
# 20041001: version bump
OLD_LIBS+=lib/libreadline.so.4
OLD_LIBS+=usr/lib/libhistory.so.4
OLD_LIBS+=usr/lib/libopie.so.2
OLD_LIBS+=usr/lib/libpcap.so.2
# 20040925: bind9 import
OLD_LIBS+=usr/lib/libisc.so.1
# 200408XX
OLD_LIBS+=usr/lib/snmp_netgraph.so.1
.if ${MACHINE_ARCH} != "alpha" && ${MACHINE_ARCH} != "sparc64"
# 20040130: libkse renamed to libpthread
OLD_LIBS+=usr/lib/libkse.so.1
.endif
# 200404XX
OLD_LIBS+=usr/lib/libsnmp.so.1
OLD_LIBS+=usr/lib/snmp_mibII.so.1
# 200309XX
OLD_LIBS+=usr/lib/libasn1.so.6
OLD_LIBS+=usr/lib/libhdb.so.6
OLD_LIBS+=usr/lib/libkadm5clnt.so.6
OLD_LIBS+=usr/lib/libkadm5srv.so.6
OLD_LIBS+=usr/lib/libkrb5.so.6
OLD_LIBS+=usr/lib/libroken.so.6
# 200304XX
OLD_LIBS+=usr/lib/libc.so.4
OLD_LIBS+=usr/lib/libc_r.so.4
OLD_LIBS+=usr/lib/libdevstat.so.2
OLD_LIBS+=usr/lib/libedit.so.3
OLD_LIBS+=usr/lib/libgmp.so.3
OLD_LIBS+=usr/lib/libmp.so.3
OLD_LIBS+=usr/lib/libpam.so.1
OLD_LIBS+=usr/lib/libposix1e.so.2
OLD_LIBS+=usr/lib/libskey.so.2
OLD_LIBS+=usr/lib/libusbhid.so.0
OLD_LIBS+=usr/lib/libvgl.so.2
# 200302XX
OLD_LIBS+=usr/lib/libacl.so.3
OLD_LIBS+=usr/lib/libasn1.so.5
OLD_LIBS+=usr/lib/libcrypto.so.2
OLD_LIBS+=usr/lib/libgssapi.so.5
OLD_LIBS+=usr/lib/libhdb.so.5
OLD_LIBS+=usr/lib/libkadm.so.3
OLD_LIBS+=usr/lib/libkadm5clnt.so.5
OLD_LIBS+=usr/lib/libkadm5srv.so.5
OLD_LIBS+=usr/lib/libkafs.so.3
OLD_LIBS+=usr/lib/libkafs5.so.5
OLD_LIBS+=usr/lib/libkdb.so.3
OLD_LIBS+=usr/lib/libkrb.so.3
OLD_LIBS+=usr/lib/libroken.so.
OLD_LIBS+=usr/lib/libssl.so.2
OLD_LIBS+=usr/lib/pam_kerberosIV.so
# 200203XX
OLD_LIBS+=usr/lib/libss.so.3
OLD_LIBS+=usr/lib/libusb.so.0
# 200112XX
OLD_LIBS+=usr/lib/libfetch.so.2
# 200104XX
OLD_LIBS+=usr/lib/libdescrypt.so.2
OLD_LIBS+=usr/lib/libscrypt.so.2
# 200102XX
OLD_LIBS+=usr/lib/libcrypto.so.1
OLD_LIBS+=usr/lib/libssl.so.1
# 200009XX
OLD_LIBS+=usr/lib/libRSAglue.so.1
OLD_LIBS+=usr/lib/librsaINTL.so.1
OLD_LIBS+=usr/lib/librsaUSA.so.1
# 200006XX
OLD_LIBS+=usr/lib/libalias.so.3
OLD_LIBS+=usr/lib/libfetch.so.1
OLD_LIBS+=usr/lib/libipsec.so.0
# 200005XX
OLD_LIBS+=usr/lib/libxpg4.so.2
# 200002XX
OLD_LIBS+=usr/lib/libc.so.3
OLD_LIBS+=usr/lib/libcurses.so.2
OLD_LIBS+=usr/lib/libdialog.so.3
OLD_LIBS+=usr/lib/libedit.so.2
OLD_LIBS+=usr/lib/libf2c.so.2
OLD_LIBS+=usr/lib/libftpio.so.4
OLD_LIBS+=usr/lib/libg++.so.4
OLD_LIBS+=usr/lib/libhistory.so.3
OLD_LIBS+=usr/lib/libmytinfo.so.2
OLD_LIBS+=usr/lib/libncurses.so.3
OLD_LIBS+=usr/lib/libreadline.so.3
OLD_LIBS+=usr/lib/libss.so.2
OLD_LIBS+=usr/lib/libtermcap.so.2
OLD_LIBS+=usr/lib/libutil.so.2
OLD_LIBS+=usr/lib/libvgl.so.1
OLD_LIBS+=usr/lib/libwrap.so.2
# 199909XX
OLD_LIBS+=usr/lib/libc_r.so.3

OLD_DIRS+=

