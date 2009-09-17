#!/usr/bin/perl -w
#
##########################################################################
# Convert kernel-doc style comments to Doxygen comments.
##########################################################################
#
# This script reads a C source file from stdin, and writes
# to stdout.  Normal usage:
#
# $ mv file.c file.c.gtkdoc
# $ kerneldoc2doxygen.pl <file.c.gtkdoc >file.c
#
# Or to do the same thing with multiple files:
# $ perl -i.gtkdoc kerneldoc2doxygen.pl *.c *.h
#
# This script may also be suitable for use as a Doxygen input filter,
# but that has not been tested.
#
# Back up your source files before using this script!!
#
##########################################################################
# Copyright (C) 2003 Jonathan Foster <jon@jon-foster.co.uk>
# Copyright (C) 2005-2008 Jouni Malinen <j@w1.fi>
# (modified for kerneldoc format used in wpa_supplicant)
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License version 2 as
# published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
# or look at http://www.gnu.org/licenses/gpl.html
##########################################################################


##########################################################################
#
# This function converts a single comment from gtk-doc to Doxygen format.
# The parameter does not include the opening or closing lines
# (i.e. given a comment like this:
#    "/**\n"
#    " * FunctionName:\n"
#    " * @foo: This describes the foo parameter\n"
#    " * @bar: This describes the bar parameter\n"
#    " * @Returns: This describes the return value\n"
#    " *\n"
#    " * This describes the function.\n"
#    " */\n"
# This function gets:
#    " * FunctionName:\n"
#    " * @foo: This describes the foo parameter\n"
#    " * @bar: This describes the bar parameter\n"
#    " * @Returns: This describes the return value\n"
#    " *\n"
#    " * This describes the function.\n"
# And it returns:
#    " * This describes the function.\n"
#    " *\n"
#    " * @param foo This describes the foo parameter\n"
#    " * @param bar This describes the bar parameter\n"
#    " * @return This describes the return value\n"
# )
#
sub fixcomment {
    $t = $_[0];

    # wpa_supplicant -> %wpa_supplicant except for struct wpa_supplicant
    $t =~ s/struct wpa_supplicant/struct STRUCTwpa_supplicant/sg;
    $t =~ s/ wpa_supplicant/ \%wpa_supplicant/sg;
    $t =~ s/struct STRUCTwpa_supplicant/struct wpa_supplicant/sg;

    # " * func: foo" --> "\brief foo\n"
    # " * struct bar: foo" --> "\brief foo\n"
    # If this fails, not a kernel-doc comment ==> return unmodified.
    ($t =~ s/^[\t ]*\*[\t ]*(struct )?([^ \t\n]*) - ([^\n]*)/\\brief $3\n/s)
      or return $t;

    # " * Returns: foo" --> "\return foo"
    $t =~ s/\n[\t ]*\*[\t ]*Returns:/\n\\return/sig;

    # " * @foo: bar" --> "\param foo bar"
    # Handle two common typos: No ":", or "," instead of ":".
    $t =~ s/\n[\t ]*\*[\t ]*\@([^ :,]*)[:,]?[\t ]*/\n\\param $1 /sg;

    return $t;
}

##########################################################################
# Start of main code

# Read entire stdin into memory - one multi-line string.
$_ = do { local $/; <> };

s{^/\*\n \*}{/\*\* \\file\n\\brief};
s{ \* Copyright}{\\par Copyright\nCopyright};

# Fix any comments like "/*************" so they don't match.
# "/***" ===> "/* *"
s{/\*\*\*}{/\* \*}gs;

# The main comment-detection code.
s{
    (               # $1 = Open comment
        /\*\*       # Open comment
        (?!\*)      # Do not match /*** (redundant due to fixup above).
        [\t ]*\n?   # If 1st line is whitespace, match the lot (including the newline).
    )
    (.*?)           # $2 = Body of comment (multi-line)
    (               # $3 = Close comment
        (           # If possible, match the whitespace before the close-comment
            (?<=\n) # This part only matches after a newline
            [\t ]*  # Eat whitespace
        )?
        \*/         # Close comment
    )
 }
 {
    $1 . fixcomment($2) . $3
 }gesx;
# ^^^^ Modes: g - Global, match all occurances.
#             e - Evaluate the replacement as an expression.
#             s - Single-line - allows the pattern to match across newlines.
#             x - eXtended pattern, ignore embedded whitespace
#                 and allow comments.

# Write results to stdout
print $_;

