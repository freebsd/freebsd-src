#
# Makefile for the FAQ/printcap
#

PROG = hpf
SRCS = hpf.c 


install:
	if test -d /usr/libexec/lpr; \
	then \
	true; \ 
	else \
	mkdir /usr/libexec/lpr;
	fi;

	install -c -o bin -g bin -m 555 hpf /usr/libexec/lpr
	install -c -o bin -g bin -m 555 ps2lj3 /usr/libexec/lpr
	install -c -o bin -g bin -m 555 printcap.sample /etc/printcap

.include <bsd.prog.mk>

