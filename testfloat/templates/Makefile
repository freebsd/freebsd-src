
PROCESSOR_H = ../../processors/!!!processor.h
SOFTFLOAT_VERSION = bits64
TARGET = !!!target
SOFTFLOAT_DIR = ../../softfloat/$(SOFTFLOAT_VERSION)/$(TARGET)

OBJ = .o
EXE =
INCLUDES = -I. -I.. -I$(SOFTFLOAT_DIR)
COMPILE_C = gcc -c -o $@ $(INCLUDES) -I- -O2
COMPILE_SLOWFLOAT_C = gcc -c -o $@ $(INCLUDES) -I- -O3
LINK = gcc -o $@

SOFTFLOAT_H = $(SOFTFLOAT_DIR)/softfloat.h
SOFTFLOAT_OBJ = $(SOFTFLOAT_DIR)/softfloat$(OBJ)

ALL: testsoftfloat$(EXE) testfloat$(EXE)

systmodes$(OBJ): milieu.h ../systmodes.h systmodes.c
	$(COMPILE_C) systmodes.c

systflags$(OBJ): milieu.h ../systflags.h systflags.c
	$(COMPILE_C) systflags.c

systfloat$(OBJ): milieu.h $(SOFTFLOAT_H) ../systfloat.h ../systfloat.c
	$(COMPILE_C) ../systfloat.c

#------------------------------------------------------------------------------
# Probably O.K. below here.
#------------------------------------------------------------------------------

milieu.h: $(PROCESSOR_H)
	touch milieu.h

fail$(OBJ): milieu.h ../fail.h
	$(COMPILE_C) ../fail.c

random$(OBJ): milieu.h ../random.h
	$(COMPILE_C) ../random.c

testCases$(OBJ): milieu.h ../fail.h ../random.h $(SOFTFLOAT_H) ../testCases.h ../testCases.c
	$(COMPILE_C) ../testCases.c

writeHex$(OBJ): milieu.h $(SOFTFLOAT_H) ../writeHex.h ../writeHex.c
	$(COMPILE_C) ../writeHex.c

testLoops$(OBJ): milieu.h $(SOFTFLOAT_H) ../testCases.h ../writeHex.h ../testLoops.h ../testLoops.c
	$(COMPILE_C) ../testLoops.c

slowfloat$(OBJ): milieu.h $(SOFTFLOAT_H) ../slowfloat.h ../slowfloat-32.c ../slowfloat-64.c ../slowfloat.c
	$(COMPILE_SLOWFLOAT_C) ../slowfloat.c

testsoftfloat$(OBJ): milieu.h ../fail.h $(SOFTFLOAT_H) ../testCases.h ../testLoops.h ../slowfloat.h ../testsoftfloat.c
	$(COMPILE_C) ../testsoftfloat.c

testsoftfloat$(EXE): fail$(OBJ) random$(OBJ) $(SOFTFLOAT_OBJ) testCases$(OBJ) writeHex$(OBJ) testLoops$(OBJ) slowfloat$(OBJ) testsoftfloat$(OBJ)
	$(LINK) fail$(OBJ) random$(OBJ) $(SOFTFLOAT_OBJ) testCases$(OBJ) writeHex$(OBJ) testLoops$(OBJ) slowfloat$(OBJ) testsoftfloat$(OBJ)

testFunction$(OBJ): milieu.h $(SOFTFLOAT_H) ../testCases.h ../testLoops.h ../systmodes.h ../systflags.h systfloat.h ../testFunction.h ../testFunction.c
	$(COMPILE_C) ../testFunction.c

testfloat$(OBJ): milieu.h ../fail.h $(SOFTFLOAT_H) ../testCases.h ../testLoops.h ../systflags.h ../testFunction.h testfloat.c
	$(COMPILE_C) testfloat.c

testfloat$(EXE): fail$(OBJ) random$(OBJ) $(SOFTFLOAT_OBJ) testCases$(OBJ) writeHex$(OBJ) testLoops$(OBJ) systmodes$(OBJ) systflags$(OBJ) systfloat$(OBJ) testFunction$(OBJ) testfloat$(OBJ)
	$(LINK) fail$(OBJ) random$(OBJ) $(SOFTFLOAT_OBJ) testCases$(OBJ) writeHex$(OBJ) testLoops$(OBJ) systmodes$(OBJ) systflags$(OBJ) systfloat$(OBJ) testFunction$(OBJ) testfloat$(OBJ)

