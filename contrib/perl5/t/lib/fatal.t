#!./perl -w

BEGIN {
   chdir 't' if -d 't';
   @INC = '../lib';
   print "1..15\n";
}

use strict;
use Fatal qw(open close :void opendir);

my $i = 1;
eval { open FOO, '<lkjqweriuapofukndajsdlfjnvcvn' };
print "not " unless $@ =~ /^Can't open/;
print "ok $i\n"; ++$i;

my $foo = 'FOO';
for ('$foo', "'$foo'", "*$foo", "\\*$foo") {
    eval qq{ open $_, '<$0' };
    print "not " if $@;
    print "ok $i\n"; ++$i;

    print "not " if $@ or scalar(<$foo>) !~ m|^#!./perl|;
    print "ok $i\n"; ++$i;
    eval qq{ close FOO };
    print "not " if $@;
    print "ok $i\n"; ++$i;
}

eval { opendir FOO, 'lkjqweriuapofukndajsdlfjnvcvn' };
print "not " unless $@ =~ /^Can't open/;
print "ok $i\n"; ++$i;

eval { my $a = opendir FOO, 'lkjqweriuapofukndajsdlfjnvcvn' };
print "not " if $@ =~ /^Can't open/;
print "ok $i\n"; ++$i;
