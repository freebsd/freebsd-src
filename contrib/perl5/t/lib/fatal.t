#!./perl -w

BEGIN {
   chdir 't' if -d 't';
   unshift @INC, '../lib';
   print "1..9\n";
}

use strict;
use Fatal qw(open);

my $i = 1;
eval { open FOO, '<lkjqweriuapofukndajsdlfjnvcvn' };
print "not " unless $@ =~ /^Can't open/;
print "ok $i\n"; ++$i;

my $foo = 'FOO';
for ('$foo', "'$foo'", "*$foo", "\\*$foo") {
    eval qq{ open $_, '<$0' };
    print "not " if $@;
    print "ok $i\n"; ++$i;

    print "not " unless scalar(<FOO>) =~ m|^#!./perl|;
    print "not " if $@;
    print "ok $i\n"; ++$i;
    close FOO;
}
