PROG=   ipfw
DPADD=  ${LIBKVM}
LDADD=  -lkvm

MAN8=	ipfw.8

test:	ipfw
	./ipfw add reject tcp from any to any
	./ipfw add 11 reject tcp from 1.2.3.4 to 5.6.7.8 established
	./ipfw add 11 reject tcp from 1.2.3.4 to 5.6.7.8/24 in
	./ipfw add 11 reject tcp from 1.2.3.4 to 5.6.7.8 in out
	./ipfw add 11 reject tcp from 1.2.3.4 to 5.6.7.8 frag out
	./ipfw add 11 reject tcp from 1.2.3.4 to 5.6.7.8 tcpf p,f,!a ipopt !ts
	./ipfw add 12 count log udp from 1.2.3.4 to 5.6.7.8 123-125,234,245

.include <bsd.prog.mk>
