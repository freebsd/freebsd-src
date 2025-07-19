# $NetBSD: varname-circumflex.mk,v 1.1 2025/06/27 20:20:56 rillig Exp $
#
# Tests for the target-local variable "^", which is required by POSIX 2024
# and provided by GNU make.

# TODO: Support $^.

all: .PHONY
all: no_prerequisites prerequisite
all: unique duplicate
all: dir_part file_part
all: implicit.tout
all: wait

.if defined(^)
.  error
.endif

no_prerequisites:
	@echo $@: $^

prerequisite: file1.o
	@echo $@: $^

unique: file1.o file2.o file3.o
	@echo $@: $^

duplicate: file1.o file2.o file3.o file3.o
	@echo $@: $^

dir_part: /usr/include/stdio.h /usr/include/unistd.h foo.h
	@echo $@: $(^D)

file_part: /usr/include/stdio.h /usr/include/unistd.h foo.h
	@echo $@: ${^F}

wait: file1.o .WAIT file2.o
	@echo $@: $^

.SUFFIXES:
.SUFFIXES: .tin .tout

.tin.tout:
	@echo $@: $^

file1.o file2.o file3.o:
/usr/include/stdio.h /usr/include/unistd.h foo.h implicit.tin:
