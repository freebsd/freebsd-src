#	$Id: AIX.m4,v 8.2 1999-05-28 05:54:26 gshapiro Exp $
depend: ${BEFORE} ${LINKS}
	@mv Makefile Makefile.old
	@sed -e '/^# Do not edit or remove this line or anything below it.$$/,$$d' < Makefile.old > Makefile
	@echo "# Do not edit or remove this line or anything below it." >> Makefile
	changequote([,])
	for i in ${SRCS}; \
	do \
		${CC} -M -E ${COPTS} $$i > /dev/null; \
		cat `basename $$i .c`.u >> Makefile ; \
		rm -f `basename $$i .c`.u ; \
	done;
	changequote

#	End of $RCSfile: AIX.m4,v $
