# $FreeBSD$
# This test checks the code reading archive files. The archive file
# is a BSD4.4 file with __.SYMTAB and #1/N long file names.
#
# The modification of the archive consists in slashes appended to short names.
#

all:
	-@make test_ok1 && echo "1 ok"
	-@make test_ok2 && echo "2 ok"
	-@make test_ok3 && echo "3 ok"
	-@make test_ok4 && echo "4 ok"
	-@make test_fail5 || echo "5 ok"
	-@make test_fail6 || echo "6 ok"
	-@make test_fail7 || echo "7 ok"

#############################################################################

test_ok1: libtest.a(short.o)
	@:

libtest.a(short.o): ood
	@:

#############################################################################

test_ok2: libtest.a(exactly15char.o)
	@:

libtest.a(exactly15char.o): ood
	@:

#############################################################################

test_ok3: libtest.a(exactly16chars.o)
	@:

libtest.a(exactly16chars.o): ood
	@:

#############################################################################

test_ok4: libtest.a(verylongobjectname.o)
	@:

libtest.a(verylongobjectname.o): ood
	@:

#############################################################################

# Truncated to 16 characters
test_fail5: libtest.a(verylongobjectna)
	@:

#############################################################################

# Truncated to 15 characters
test_fail6: libtest.a(verylongobjectn)
	@:

#############################################################################

test_fail7: libtest.a(\#1/20)
	@:

ood:
