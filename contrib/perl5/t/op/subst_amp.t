#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    require Config; import Config;
}

print "1..13\n";

$_ = 'x' x 20; 
s/\d*|x/<$&>/g; 
$foo = '<>' . ('<x><>' x 20) ;
print ($_ eq $foo ? "ok 1\n" : "not ok 1\n#'$_'\n#'$foo'\n");

$t = 'aaa';

$_ = $t;
@res = ();
pos = 1;
s/\Ga(?{push @res, $_, $`})/xx/g;
print "not " unless "$_ @res" eq 'axxxx aaa a aaa aa';
print "ok 2\n";

$_ = $t;
@res = ();
pos = 1;
s/\Ga(?{push @res, $_, $`})/x/g;
print "not " unless "$_ @res" eq 'axx aaa a aaa aa';
print "ok 3\n";

$_ = $t;
@res = ();
pos = 1;
s/\Ga(?{push @res, $_, $`})/xx/;
print "not " unless "$_ @res" eq 'axxa aaa a';
print "ok 4\n";

$_ = $t;
@res = ();
pos = 1;
s/\Ga(?{push @res, $_, $`})/x/;
print "not " unless "$_ @res" eq 'axa aaa a';
print "ok 5\n";

$a = $t;
@res = ();
pos ($a) = 1;
$a =~ s/\Ga(?{push @res, $_, $`})/xx/g;
print "#'$a' '@res'\nnot " unless "$a @res" eq 'axxxx aaa a aaa aa';
print "ok 6\n";

$a = $t;
@res = ();
pos ($a) = 1;
$a =~ s/\Ga(?{push @res, $_, $`})/x/g;
print "#'$a' '@res'\nnot " unless "$a @res" eq 'axx aaa a aaa aa';
print "ok 7\n";

$a = $t;
@res = ();
pos ($a) = 1;
$a =~ s/\Ga(?{push @res, $_, $`})/xx/;
print "#'$a' '@res'\nnot " unless "$a @res" eq 'axxa aaa a';
print "ok 8\n";

$a = $t;
@res = ();
pos ($a) = 1;
$a =~ s/\Ga(?{push @res, $_, $`})/x/;
print "#'$a' '@res'\nnot " unless "$a @res" eq 'axa aaa a';
print "ok 9\n";

sub x2 {'xx'}
sub x1 {'x'}

$a = $t;
@res = ();
pos ($a) = 1;
$a =~ s/\Ga(?{push @res, $_, $`})/x2/ge;
print "#'$a' '@res'\nnot " unless "$a @res" eq 'axxxx aaa a aaa aa';
print "ok 10\n";

$a = $t;
@res = ();
pos ($a) = 1;
$a =~ s/\Ga(?{push @res, $_, $`})/x1/ge;
print "#'$a' '@res'\nnot " unless "$a @res" eq 'axx aaa a aaa aa';
print "ok 11\n";

$a = $t;
@res = ();
pos ($a) = 1;
$a =~ s/\Ga(?{push @res, $_, $`})/x2/e;
print "#'$a' '@res'\nnot " unless "$a @res" eq 'axxa aaa a';
print "ok 12\n";

$a = $t;
@res = ();
pos ($a) = 1;
$a =~ s/\Ga(?{push @res, $_, $`})/x1/e;
print "#'$a' '@res'\nnot " unless "$a @res" eq 'axa aaa a';
print "ok 13\n";

