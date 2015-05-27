##===- unittests/Makefile ----------------------------------*- Makefile -*-===##
#
#                     The LLVM Compiler Infrastructure
#
# This file is distributed under the University of Illinois Open Source
# License. See LICENSE.TXT for details.
#
##===----------------------------------------------------------------------===##

LEVEL = ..

PARALLEL_DIRS = ADT Analysis AsmParser Bitcode CodeGen DebugInfo \
                ExecutionEngine IR LineEditor Linker MC Option ProfileData \
                Support Transforms

include $(LEVEL)/Makefile.config
include $(LLVM_SRC_ROOT)/unittests/Makefile.unittest

clean::
	$(Verb) $(RM) -f *Tests
