#!./perl

$^I = 'bak*';

# Modified from the original inplace.t to test adding prefixes

print "1..2\n";

@ARGV = ('.a','.b','.c');
if ($^O eq 'MSWin32') {
  $CAT = '.\perl -e "print<>"';
  `.\\perl -le "print 'foo'" > .a`;
  `.\\perl -le "print 'foo'" > .b`;
  `.\\perl -le "print 'foo'" > .c`;
}
elsif ($^O eq 'VMS') {
  $CAT = 'MCR []perl. -e "print<>"';
  `MCR []perl. -le "print 'foo'" > ./.a`;
  `MCR []perl. -le "print 'foo'" > ./.b`;
  `MCR []perl. -le "print 'foo'" > ./.c`;
}
else {
  $CAT = 'cat';
  `echo foo | tee .a .b .c`;
}
while (<>) {
    s/foo/bar/;
}
continue {
    print;
}

if (`$CAT .a .b .c` eq "bar\nbar\nbar\n") {print "ok 1\n";} else {print "not ok 1\n";}
if (`$CAT bak.a bak.b bak.c` eq "foo\nfoo\nfoo\n") {print "ok 2\n";} else {print "not ok 2\n";}

unlink '.a', '.b', '.c', 'bak.a', 'bak.b', 'bak.c';
