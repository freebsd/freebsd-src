# $NetBSD: parse.mk,v 1.8 2025/06/28 22:39:29 rillig Exp $
#
# Test those parts of the parsing that do not belong in any of the other
# categories.

# expect+1: Invalid line "<<<<<< old"
<<<<<< old

# No diagnostic since the following line is parsed as a variable assignment,
# even though the variable name is empty.  See also varname-empty.mk.
====== middle

# expect+1: Invalid line ">>>>>> new"
>>>>>> new


# Since parse.c 1.578 from 2021-12-14 and before parse.c 1.681 from
# 2022-07-24, if a line of a makefile could only be a dependency specification
# but didn't contain any of the dependency operators ':', '!', '::' and its
# expansion ended with a space, make read a single byte from the memory beyond
# the expanded line's terminating '\0'.
#
# https://bugs.freebsd.org/265119
# expect+1: Invalid line "one-target ${:U }", expanded to "one-target  "
one-target ${:U }


# Since parse.c 1.656 from 2022-01-27 and before parse.c 1.662 from
# 2022-02-05, there was an out-of-bounds read in Parse_IsVar when looking for
# a variable assignment in a dependency line with trailing whitespace.  Lines
# without trailing whitespace were not affected.  Global variable assignments
# were guaranteed to have no trailing whitespace and were thus not affected.
#
# Try to reproduce some variants that may lead to a crash, depending on the
# memory allocator.  To get a crash, the terminating '\0' of the line must be
# the last byte of a memory page.  The expression '${:U}' forces this trailing
# whitespace.

# On FreeBSD x86_64, a crash could in some cases be forced using the following
# line, which has length 47, and if the memory for the expanded line starts at
# 0xXXXX_XXd0, the terminating '\0' may end up at 0xXXXX_Xfff:
Try_to_crash_FreeBSD.xxxxxxxxxxxxxxxxxx: 12345 ${:U}

# The following line has length 4095 after being expanded, so line[4095] ==
# '\0'.  If the line is
# allocated on a page boundary and the following page is not mapped, this line
# leads to a segmentation fault.
${:U:range=511:@_@1234567@:ts.}: 12345 ${:U}

# The following line has length 8191, so line[8191] == '\0'.  If the line is
# allocated on a page boundary and the following page is not mapped, this line
# leads to a segmentation fault.
${:U:range=1023:@_@1234567@:ts.}: 12345 ${:U}

12345:
