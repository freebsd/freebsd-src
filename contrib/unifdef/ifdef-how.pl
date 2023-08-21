#!/usr/bin/perl

use warnings;
use strict;

if (@ARGV != 2) {
	die <<END;
usage: ifdef-how <file> <line>

Print the sequence of preprocessor conditionals which lead to the
given line being retained after preprocessing. There is no output
if the line is always retained. Conditionals that must be true are
printed verbatim; conditionals that musy be false have their
preprocessor keyword prefixed with NOT.

Warning: this program does not parse comments or strings, so it will
not handle tricky code correctly.
END
}

my $file = shift;
my $line = shift;

open my $fh, '<', $file
    or die "ifdef-how: open $file: $!\n";

my @stack;
while (<$fh>) {
	last if $. == $line;
	if (m{^\s*#\s*(if|ifdef|ifndef)\b}) {
		push @stack, $_;
	}
	if (m{^\s*#\s*(elif|else)\b}) {
		$stack[-1] =~ s{^(\s*#\s*)(?!NOT)\b}{${1}NOT}gm;
		$stack[-1] .= $_;
	}
	if (m{^\s*#\s*endif\b}) {
		pop @stack;
	}
}

print @stack;
