#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..3\n";

use SelectSaver;

open(FOO, ">foo-$$") || die;

print "ok 1\n";
{
    my $saver = new SelectSaver(FOO);
    print "foo\n";
}

# Get data written to file
open(FOO, "foo-$$") || die;
chomp($foo = <FOO>);
close FOO;
unlink "foo-$$";

print "ok 2\n" if $foo eq "foo";

print "ok 3\n";
