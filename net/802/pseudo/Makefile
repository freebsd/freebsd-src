all:	pseudocode.h actionnm.h

clean:
	touch pseudocode.h actionnm.h
	rm pseudocode.h actionnm.h

pseudocode.h: pseudocode opcd2num.sed compile.awk
	sed -f opcd2num.sed pseudocode |  awk -f compile.awk >pseudocode.h

actionnm.h: pseudocode.h actionnm.awk
	awk -f actionnm.awk pseudocode.h>actionnm.h

	
