#	$Id: NCR.m4,v 8.6 1999-05-27 22:03:29 peterh Exp $
depend: ${BEFORE} ${LINKS}
	@mv Makefile Makefile.old
	@sed -e '/^# Do not edit or remove this line or anything below it.$$/,$$d' < Makefile.old > Makefile
	@echo "# Do not edit or remove this line or anything below it." >> Makefile
	${CC} -w0 -Hmake ${COPTS} ${SRCS} >> Makefile

#	End of $RCSfile: NCR.m4,v $
