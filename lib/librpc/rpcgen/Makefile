#
# @(#)Makefile	2.1 88/08/01 4.0 RPCSRC
#
# Makefile for rpc protocol compiler
# Copyright (C) 1987, Sun Microsystems, Inc.
#
SRCS= rpc_main.c rpc_hout.c rpc_cout.c rpc_parse.c rpc_scan.c rpc_util.c \
	rpc_svcout.c rpc_clntout.c
HDRS= rpc_util.h rpc_parse.h rpc_scan.h
OBJS= rpc_main.o rpc_hout.o rpc_cout.o rpc_parse.o rpc_scan.o rpc_util.o \
	rpc_svcout.o rpc_clntout.o

GOAL=rpcgen
CFLAGS = -O
DESTDIR=

$(GOAL): $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@

install: $(GOAL)
	@echo "Installing the RPC Protocol Compiler"
	install -s $(GOAL) $(DESTDIR)/usr/bin

lint: $(SRCS) $(HDRS)
	lint $(SRCS)

clean:
	rm -f $(GOAL) $(OBJS)

depend: $(SRCS) $(HDRS)
	@${CC} ${CFLAGS} -M ${SRCS} > makedep
	@echo '/^# DO NOT DELETE THIS LINE/+1,$$d' >eddep
	@echo '$$r makedep' >>eddep
	@echo 'w' >>eddep
	@cp Makefile makefile.bak
	@ed - Makefile < eddep
	@rm eddep makedep makefile.bak

depend.42BSD depend.42bsd:
	cp /dev/null x.c
	for i in $(SRCS) ; do \
              (/bin/grep '^#[         ]*include' x.c $$i | sed \
                      -e '/\.\.\/h/d' \
                      -e '/\.\.\/ufs/d' \
                      -e 's,<\(.*\)>,"/usr/include/\1",' \
                      -e 's/:[^"]*"\([^"]*\)".*/: \1/' \
	                  -e 's/\.c/\.o/' >>makedep); done
	echo '/^# DO NOT DELETE THIS LINE/+2,$$d' >eddep
	echo '$$r makedep' >>eddep
	echo 'w' >>eddep
	cp Makefile Makefile.bak
	ed - Makefile < eddep
	rm eddep makedep x.c
	echo '# DEPENDENCIES MUST END AT END OF FILE' >> Makefile
	echo '# IF YOU PUT STUFF HERE IT WILL GO AWAY' >> Makefile
	echo '# see make depend above' >> Makefile

# DO NOT DELETE THIS LINE


