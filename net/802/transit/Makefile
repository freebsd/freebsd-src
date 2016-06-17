include $(TOPDIR)/Rules.make

all:	pdutr.h timertr.h

pdutr.h: pdutr.pre compile.awk			
	awk -f ./compile.awk pdutr.pre > pdutr.h

timertr.h: timertr.pre compile.awk
	awk -f ./compile.awk timertr.pre > timertr.h

clean:
	touch pdutr.h timertr.h
	rm pdutr.h timertr.h
