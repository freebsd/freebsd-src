#!/bin/sh
#
#	ad2c : Convert app-defaults file to C strings decls.
#
#	George Ferguson, ferguson@cs.rcohester.edu, 12 Nov 1990.
#	19 Mar 1991: gf
#		Made it self-contained.
#	6 Jan 1992: mycroft@gnu.ai.mit.edu (Charles Hannum)
#		Removed use of "-n" and ":read" label since Gnu and
#		IBM sed print pattern space on "n" command. Still works
#		with Sun sed, of course.
#	7 Jan 1992: matthew@sunpix.East.Sun.COM (Matthew Stier)
#		Escape quotes after escaping backslashes.
#	8 Jul 1992: Version 1.6
#		Manpage fixes.
#	19 Apr 1993: Version 1.7
#		Remove comments that were inside the sed command since
#		some versions of sed don't like them. The comments are
#		now given here in the header.
#
# Comments on the script by line:
# /^!/d		Remove comments
# /^$/d		Remove blanks
# s/\\/\\\\/g	Escape backslashes...
# s/\\$//g	...except the line continuation ones
# s/"/\\"/g	Escape quotes
# s/^/"/	Add leading quote
# : test	Establish label for later branch
# /\\$/b slash	Branch to label "slash" if line ends in backslash
# s/$/",/	Otherwise add closing quote and comma...
# p		...output the line...
# d		...and clear the pattern space so it's not printed again
# : slash	Branch comes here if line ends in backslash
# n		Read next line, append to pattern space
# [...]		The "d" and "s" commands that follow just delete
#		comments and blank lines and escape control sequences
# b test	Branch up to see if the line ends in backslash or not
#

sed '
/^!/d
/^$/d
s/\\/\\\\/g
s/\\$//g
s/"/\\"/g
s/^/"/
: test
/\\$/b slash
s/$/",/
p
d
: slash
n
/^!/d
/^$/d
s/"/\\"/g
s/\\\\/\\/g
s/\\n/\\\\n/g
s/\\t/\\\\t/g
s/\\f/\\\\f/g
s/\\b/\\\\b/g
b test' "$@"
