#!./perl

#
# Ensure that syntax using colons (:) is parsed correctly.
# The tests are done on the following tokens (by default):
# ABC LABEL XYZZY m q qq qw qx s tr y AUTOLOAD and alarm 
#	-- Robin Barker <rmb@cise.npl.co.uk>
#

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use strict;

$_ = '';	# to avoid undef warning on m// etc.

sub ok {
    my($test,$ok) = @_;
    print "not " unless $ok;
    print "ok $test\n";
}

$SIG{__WARN__} = sub { 1; }; # avoid some spurious warnings

print "1..25\n";

ok 1, (eval "package ABC; sub zyx {1}; 1;" and
	eval "ABC::zyx" and
	not eval "ABC:: eq ABC||" and
	not eval "ABC::: >= 0");

ok 2, (eval "package LABEL; sub zyx {1}; 1;" and
	eval "LABEL::zyx" and
	not eval "LABEL:: eq LABEL||" and
	not eval "LABEL::: >= 0");

ok 3, (eval "package XYZZY; sub zyx {1}; 1;" and
	eval "XYZZY::zyx" and
	not eval "XYZZY:: eq XYZZY||" and
	not eval "XYZZY::: >= 0");

ok 4, (eval "package m; sub zyx {1}; 1;" and
	not eval "m::zyx" and
	eval "m:: eq m||" and
	not eval "m::: >= 0");

ok 5, (eval "package q; sub zyx {1}; 1;" and
	not eval "q::zyx" and
	eval "q:: eq q||" and
	not eval "q::: >= 0");

ok 6, (eval "package qq; sub zyx {1}; 1;" and
	not eval "qq::zyx" and
	eval "qq:: eq qq||" and
	not eval "qq::: >= 0");

ok 7, (eval "package qw; sub zyx {1}; 1;" and
	not eval "qw::zyx" and
	eval "qw:: eq qw||" and
	not eval "qw::: >= 0");

ok 8, (eval "package qx; sub zyx {1}; 1;" and
	not eval "qx::zyx" and
	eval "qx:: eq qx||" and
	not eval "qx::: >= 0");

ok 9, (eval "package s; sub zyx {1}; 1;" and
	not eval "s::zyx" and
	not eval "s:: eq s||" and
	eval "s::: >= 0");

ok 10, (eval "package tr; sub zyx {1}; 1;" and
	not eval "tr::zyx" and
	not eval "tr:: eq tr||" and
	eval "tr::: >= 0");

ok 11, (eval "package y; sub zyx {1}; 1;" and
	not eval "y::zyx" and
	not eval "y:: eq y||" and
	eval "y::: >= 0");

ok 12, (eval "ABC:1" and
	not eval "ABC:echo: eq ABC|echo|" and
	not eval "ABC:echo:ohce: >= 0");

ok 13, (eval "LABEL:1" and
	not eval "LABEL:echo: eq LABEL|echo|" and
	not eval "LABEL:echo:ohce: >= 0");

ok 14, (eval "XYZZY:1" and
	not eval "XYZZY:echo: eq XYZZY|echo|" and
	not eval "XYZZY:echo:ohce: >= 0");

ok 15, (not eval "m:1" and
	eval "m:echo: eq m|echo|" and
	not eval "m:echo:ohce: >= 0");

ok 16, (not eval "q:1" and
	eval "q:echo: eq q|echo|" and
	not eval "q:echo:ohce: >= 0");

ok 17, (not eval "qq:1" and
	eval "qq:echo: eq qq|echo|" and
	not eval "qq:echo:ohce: >= 0");

ok 18, (not eval "qw:1" and
	eval "qw:echo: eq qw|echo|" and
	not eval "qw:echo:ohce: >= 0");

ok 19, (not eval "qx:1" and
	eval "qx:echo 1: eq qx|echo 1|" and	# echo without args may warn
	not eval "qx:echo:ohce: >= 0");

ok 20, (not eval "s:1" and
	not eval "s:echo: eq s|echo|" and
	eval "s:echo:ohce: >= 0");

ok 21, (not eval "tr:1" and
	not eval "tr:echo: eq tr|echo|" and
	eval "tr:echo:ohce: >= 0");

ok 22, (not eval "y:1" and
	not eval "y:echo: eq y|echo|" and
	eval "y:echo:ohce: >= 0");

ok 23, (eval "AUTOLOAD:1" and
	not eval "AUTOLOAD:echo: eq AUTOLOAD|echo|" and
	not eval "AUTOLOAD:echo:ohce: >= 0");

ok 24, (eval "and:1" and
	not eval "and:echo: eq and|echo|" and
	not eval "and:echo:ohce: >= 0");

ok 25, (eval "alarm:1" and
	not eval "alarm:echo: eq alarm|echo|" and
	not eval "alarm:echo:ohce: >= 0");
