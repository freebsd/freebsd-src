#
# $FreeBSD$
#
# Automated test suite for libpthread (pthreads).
#

# File lists.

# Tests written in C.
CTESTS := hello_d.c hello_s.c join_leak_d.c mutex_d.c sem_d.c sigsuspend_d.c \
	sigwait_d.c

# C programs that are used internally by the tests.  The build system merely
# compiles these.
BTESTS := guard_b.c hello_b.c

# Tests written in perl.
PTESTS := guard_s.pl propagate_s.pl

# Munge the file lists to their final executable names (strip the .c).
CTESTS := $(CTESTS:R)
BTESTS := $(BTESTS:R)

CPPFLAGS := -D_LIBC_R_ -D_REENTRANT
CFLAGS := -Wall -pipe -g3
LDFLAGS_A := -static
LDFLAGS_P := -pg
LDFLAGS_S :=
LIBS := -lkse

# Flags passed to verify.  "-v" or "-u" may be useful.
VERIFY = perl verify
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
.for bin in $(CTESTS) $(BTESTS)
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
.for depfile in $(CTESTS:R:S/$/&_a.d/) $(BTESTS:R:S/$/&_a.d/) \
		$(CTESTS:R:S/$/&_p.d/) $(BTESTS:R:S/$/&_p.d/) \
		$(CTESTS:R:S/$/&_s.d/) $(BTESTS:R:S/$/&_s.d/)
.if exists($(depfile))
.include "$(depfile)"
.endif
.endfor

default : check

tests_a : $(CTESTS:S/$/&_a/) $(BTESTS:S/$/&_a/)
tests_p : $(CTESTS:S/$/&_p/) $(BTESTS:S/$/&_p/)
tests_s : $(CTESTS:S/$/&_s/) $(BTESTS:S/$/&_s/)

tests : tests_a tests_p tests_s

check_a : tests_a
.for bin in $(CTESTS) $(BTESTS)
	@cp $(bin)_a $(bin)
.endfor
	@echo "Test static library:"
	@$(VERIFY) $(VFLAGS) $(CTESTS) $(PTESTS)

check_p : tests_p
.for bin in $(CTESTS) $(BTESTS)
	@cp $(bin)_p $(bin)
.endfor
	@echo "Test profile library:"
	@$(VERIFY) $(VFLAGS) $(CTESTS) $(PTESTS)

check_s : tests_s
.for bin in $(CTESTS) $(BTESTS)
	@cp $(bin)_s $(bin)
.endfor
	@echo "Test shared library:"
	@$(VERIFY) $(VFLAGS) $(CTESTS) $(PTESTS)

check : check_a check_p check_s

clean :
	rm -f *~
	rm -f *.core
	rm -f *.out
	rm -f *.perf
	rm -f *.diff
	rm -f *.gmon
	rm -f $(CTESTS) $(BTESTS)
	rm -f $(CTESTS:S/$/&_a/) $(BTESTS:S/$/&_a/)
	rm -f $(CTESTS:S/$/&_p/) $(BTESTS:S/$/&_p/)
	rm -f $(CTESTS:S/$/&_s/) $(BTESTS:S/$/&_s/)
	rm -f *.d
	rm -f *.o
