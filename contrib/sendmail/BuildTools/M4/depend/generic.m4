#	@(#)generic.m4	8.2	(Berkeley)	2/9/1998
# dependencies
#   gross overkill, and yet still not quite enough....
${OBJS}: ${SRCDIR}/sendmail.h ${SRCDIR}/conf.h

# give a null "depend" list so that the startup script will work
depend:
#	End of generic.m4
