#	@(#)Makefile	8.1 (Berkeley) 6/5/93

#MISSING: dump.5 plot.5
MAN5=	a.out.5 acct.5 core.5 dir.5 disktab.5 ethers.5 fs.5 fbtab.5 fstab.5 \
	group.5 hosts.5 link.5 networks.5 passwd.5 phones.5 printcap.5 \
	procfs.5 protocols.5 remote.5 resolver.5 services.5 shells.5 \
	stab.5 types.5 utmp.5
MAN5+= hosts.equiv.5 hosts.lpd.5
MLINKS=	fs.5 inode.5 utmp.5 wtmp.5 utmp.5 lastlog.5
MLINKS+=hosts.equiv.5 rhosts.5

clean depend lint tags:

install: maninstall

.include <bsd.prog.mk>
