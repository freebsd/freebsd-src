#!./perl

$^I = '.bak';

# $Header: /home/cvs/386BSD/ports/lang/perl/t/io/inplace.t,v 1.1.1.1 1993/08/23 21:30:05 nate Exp $

print "1..2\n";

@ARGV = ('.a','.b','.c');
`echo foo | tee .a .b .c`;
while (<>) {
    s/foo/bar/;
}
continue {
    print;
}

if (`cat .a .b .c` eq "bar\nbar\nbar\n") {print "ok 1\n";} else {print "not ok 1\n";}
if (`cat .a.bak .b.bak .c.bak` eq "foo\nfoo\nfoo\n") {print "ok 2\n";} else {print "not ok 2\n";}

unlink '.a', '.b', '.c', '.a.bak', '.b.bak', '.c.bak';
