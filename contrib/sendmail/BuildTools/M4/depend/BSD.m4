#	@(#)BSD.m4	8.3	(Berkeley)	2/9/1998
depend: ${BEFORE}
	@mv Makefile Makefile.old
	@sed -e '/^# Do not edit or remove this line or anything below it.$$/,$$d' < Makefile.old > Makefile
	@echo "# Do not edit or remove this line or anything below it." >> Makefile
	mkdep -a -f Makefile ${COPTS} *.c

#	End of BSD.m4
