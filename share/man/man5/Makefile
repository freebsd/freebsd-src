#	@(#)Makefile	8.1 (Berkeley) 6/5/93

MAN5=	a.out.0 acct.0 core.0 dir.0 disktab.0 dump.0 fs.0 fstab.0 \
	group.0 hosts.0 networks.0 passwd.0 phones.0 plot.0 printcap.0 \
	protocols.0 remote.0 resolver.0 services.0 shells.0 stab.0 types.0 \
	utmp.0
MLINKS=	fs.5 inode.5 dump.5 dumpdates.5 utmp.5 wtmp.5

all: ${MAN5}

clean depend lint tags:

cleandir:
	rm -f ${MAN5}

install: maninstall

.include <bsd.prog.mk>
