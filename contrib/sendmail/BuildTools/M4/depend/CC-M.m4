#	@(#)CC-M.m4	8.2	(Berkeley)	2/19/98
depend: ${BEFORE}
	@mv Makefile Makefile.old
	@sed -e '/^# Do not edit or remove this line or anything below it.$$/,$$d' < Makefile.old > Makefile
	@echo "# Do not edit or remove this line or anything below it." >> Makefile
	${CC} -M ${COPTS} *.c >> Makefile

#	End of CC-M.m4
