#
# $FreeBSD: src/lib/libc_r/test/Makefile,v 1.4.2.1 2000/07/17 22:18:32 jasone Exp $
#
# Automated test suite for libc_r (pthreads).
#

# File lists.

# Diff mode tests written in C.
CDTESTS := hello_d.c mutex_d.c sem_d.c sigsuspend_d.c sigwait_d.c

# Sequence mode tests written in C.
CSTESTS := hello_s.c

# C programs that are used internally by the perl-based tests.  The build
# system merely compiles these.
BTESTS := hello_b.c

# Diff mode tests written in perl.
PDTESTS :=

# Sequence mode tests written in perl.
PSTESTS := propagate_s.pl

# Munge the file lists to their final executable names (strip the .c).
CDTESTS := $(CDTESTS:R)
CSTESTS := $(CSTESTS:R)
BTESTS := $(BTESTS:R)

CPPFLAGS := -D_LIBC_R_
CFLAGS := -Wall -pipe -g3
LDFLAGS_A := -static
LDFLAGS_P := -pg
LDFLAGS_S :=
LIBS := -pthread

# Flags passed to verify.  "-v" or "-u" may be useful.
VFLAGS :=

all : default

# Only use the following suffixes, in order to avoid any strange built-in rules.
.SUFFIXES :
.SUFFIXES : .c .o .d .pl

# Clear out all paths, then set just one (default path) for the main build
# directory.
.PATH :
.PATH : .

# Build the C programs.
.for bin in $(CDTESTS) $(CSTESTS) $(BTESTS)
$(bin)_a : $(bin:S/$/&.c/)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $(bin:S/$/&.c/) -o $(@:S/$/&.o/)
	$(CC) -o $@ $(@:S/$/&.o/) $(LDFLAGS_A) $(LIBS)
	@$(SHELL) -ec "$(CC) -M $(CPPFLAGS) $(bin:S/$/&.c/) | sed \"s/\($(bin:T)\)\.o\([ :]*\)/$(bin:H:S!/!\\/!g)\/\1_a.o \2/g\" > $(@:R:S/$/&.d/)"

$(bin)_p : $(bin:S/$/&.c/)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $(bin:S/$/&.c/) -o $(@:S/$/&.o/)
	$(CC) -o $@ $(@:S/$/&.o/) $(LDFLAGS_P) $(LIBS)
	@$(SHELL) -ec "$(CC) -M $(CPPFLAGS) $(bin:S/$/&.c/) | sed \"s/\($(bin:T)\)\.o\([ :]*\)/$(bin:H:S!/!\\/!g)\/\1_p.o \2/g\" > $(@:R:S/$/&.d/)"

$(bin)_s : $(bin:S/$/&.c/)
	$(CC) $(CFLAGS) $(CPPFLAGS) -c $(bin:S/$/&.c/) -o $(@:S/$/&.o/)
	$(CC) -o $@ $(@:S/$/&.o/) $(LDFLAGS_S) $(LIBS)
	@$(SHELL) -ec "$(CC) -M $(CPPFLAGS) $(bin:S/$/&.c/) | sed \"s/\($(bin:T)\)\.o\([ :]*\)/$(bin:H:S!/!\\/!g)\/\1_s.o \2/g\" > $(@:R:S/$/&.d/)"
.endfor

# Dependency file inclusion.
.for depfile in $(CDTESTS:R:S/$/&_a.d/) $(CSTESTS:R:S/$/&_a.d/) \
		$(BTESTS:R:S/$/&_a.d/) $(CDTESTS:R:S/$/&_p.d/) \
		$(CSTESTS:R:S/$/&_p.d/) $(BTESTS:R:S/$/&_p.d/) \
		$(CDTESTS:R:S/$/&_s.d/) $(CSTESTS:R:S/$/&_s.d/) \
		$(BTESTS:R:S/$/&_s.d/)
.if exists($(depfile))
.include "$(depfile)"
.endif
.endfor

default : check

tests_a : $(CDTESTS:S/$/&_a/) $(CSTESTS:S/$/&_a/) $(BTESTS:S/$/&_a/)
tests_p : $(CDTESTS:S/$/&_p/) $(CSTESTS:S/$/&_p/) $(BTESTS:S/$/&_p/)
tests_s : $(CDTESTS:S/$/&_s/) $(CSTESTS:S/$/&_s/) $(BTESTS:S/$/&_s/)

tests : tests_a tests_p tests_s

check_a : tests_a
.for bin in $(CDTESTS) $(CSTESTS) $(BTESTS)
	@cp $(bin)_a $(bin)
.endfor
	@echo "Test static library:"
	@./verify $(VFLAGS) -- -d $(CDTESTS) $(PDTESTS) -s $(CSTESTS) $(PSTESTS)

check_p : tests_p
.for bin in $(CDTESTS) $(CSTESTS) $(BTESTS)
	@cp $(bin)_p $(bin)
.endfor
	@echo "Test profile library:"
	@./verify $(VFLAGS) -- -d $(CDTESTS) $(PDTESTS) -s $(CSTESTS) $(PSTESTS)

check_s : tests_s
.for bin in $(CDTESTS) $(CSTESTS) $(BTESTS)
	@cp $(bin)_s $(bin)
.endfor
	@echo "Test shared library:"
	@./verify $(VFLAGS) -- -d $(CDTESTS) $(PDTESTS) -s $(CSTESTS) $(PSTESTS)

check : check_a check_p check_s

clean :
	rm -f *~
	rm -f *.core
	rm -f *.out
	rm -f *.perf
	rm -f *.diff
	rm -f *.gmon
	rm -f $(CDTESTS) $(CSTESTS) $(BTESTS)
	rm -f $(CDTESTS:S/$/&_a/) $(CSTESTS:S/$/&_a/) $(BTESTS:S/$/&_a/)
	rm -f $(CDTESTS:S/$/&_p/) $(CSTESTS:S/$/&_p/) $(BTESTS:S/$/&_p/)
	rm -f $(CDTESTS:S/$/&_s/) $(CSTESTS:S/$/&_s/) $(BTESTS:S/$/&_s/)
	rm -f *.d
	rm -f *.o
