#	@(#)NCR.m4	8.3	(Berkeley)	2/19/1998
depend: ${BEFORE}
	@mv Makefile Makefile.old
	@sed -e '/^# Do not edit or remove this line or anything below it.$$/,$$d' < Makefile.old > Makefile
	@echo "# Do not edit or remove this line or anything below it." >> Makefile
	${CC} -w0 -Hmake ${COPTS} *.c >> Makefile

#	End of NCR.m4
