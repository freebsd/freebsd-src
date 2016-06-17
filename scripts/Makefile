HEADER=header.tk
TAIL=tail.tk

# Previous versions always remade kconfig.tk because they always depended
# on soundscript.  This runs fairly fast, and I can't find all the
# Config.in files to depend on anyways.  So I'll force it to remake.

kconfig.tk: dummy

kconfig.tk: ${TOPDIR}/Makefile ${TOPDIR}/arch/${ARCH}/config.in \
		tkparse ${HEADER} ${TAIL}
	@if [ -f /usr/local/bin/wish ];	then \
		echo '#!'"/usr/local/bin/wish -f" > kconfig.tk; \
	else \
		echo '#!'"/usr/bin/wish -f" > kconfig.tk; \
	fi
	cat ${HEADER} >> ./kconfig.tk
	./tkparse < ../arch/${ARCH}/config.in >> kconfig.tk
	echo "set defaults \"arch/${ARCH}/defconfig\"" >> kconfig.tk
	echo "set ARCH \"${ARCH}\"" >> kconfig.tk
	cat ${TAIL} >> kconfig.tk
	chmod 755 kconfig.tk

tkparse: tkparse.o tkcond.o tkgen.o
	${HOSTCC} -o tkparse tkparse.o tkcond.o tkgen.o

tkparse.o: tkparse.c tkparse.h

tkcond.o: tkcond.c tkparse.h

tkgen.o: tkgen.c tkparse.h

tkparse.o tkcond.o tkgen.o:
	$(HOSTCC) $(HOSTCFLAGS) -c -o $@ $(@:.o=.c)

docproc.o: docproc.c
	$(HOSTCC) $(HOSTCFLAGS) -c -o $@ $(@:.o=.c)

docproc: docproc.o
	${HOSTCC} -o docproc docproc.o

clean:
	rm -f *~ kconfig.tk *.o tkparse mkdep split-include docproc

include $(TOPDIR)/Rules.make
