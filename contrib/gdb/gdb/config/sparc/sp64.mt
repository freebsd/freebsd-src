# Target: SPARC64
# solib.o and procfs.o taken out for now.  We don't have shared libraries yet,
# and the elf version requires procfs.o but the a.out version doesn't.
# Then again, having procfs.o in a target makefile fragment seems wrong.
TDEPFILES = sparc-tdep.o
TM_FILE= tm-sp64.h

# Need gcc for long long support.
CC = gcc
