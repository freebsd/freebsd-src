#!./perl

# "This IS structured code.  It's just randomly structured."

print "1..13\n";

while ($?) {
    $foo = 1;
  label1:
    $foo = 2;
    goto label2;
} continue {
    $foo = 0;
    goto label4;
  label3:
    $foo = 4;
    goto label4;
}
goto label1;

$foo = 3;

label2:
print "#1\t:$foo: == 2\n";
if ($foo == 2) {print "ok 1\n";} else {print "not ok 1\n";}
goto label3;

label4:
print "#2\t:$foo: == 4\n";
if ($foo == 4) {print "ok 2\n";} else {print "not ok 2\n";}

$PERL = ($^O eq 'MSWin32') ? '.\perl' : './perl';
$x = `$PERL -e "goto foo;" 2>&1`;
if ($x =~ /DCL-W-NOCOMD/) { $x = `\$ mcr sys\$disk:[]perl. -e "goto foo;"`; }

if ($x =~ /label/) {print "ok 3\n";} else {print "not ok 3\n";}

sub foo {
    goto bar;
    print "not ok 4\n";
    return;
bar:
    print "ok 4\n";
}

&foo;

sub bar {
    $x = 'bypass';
    eval "goto $x";
}

&bar;
exit;

FINALE:
print "ok 13\n";
exit;

bypass:
print "ok 5\n";

# Test autoloading mechanism.

sub two {
    ($pack, $file, $line) = caller;	# Should indicate original call stats.
    print "@_ $pack $file $line" eq "1 2 3 main $FILE $LINE"
	? "ok 7\n"
	: "not ok 7\n";
}

sub one {
    eval <<'END';
    sub one { print "ok 6\n"; goto &two; print "not ok 6\n"; }
END
    goto &one;
}

$FILE = __FILE__;
$LINE = __LINE__ + 1;
&one(1,2,3);

$wherever = NOWHERE;
eval { goto $wherever };
print $@ =~ /Can't find label NOWHERE/ ? "ok 8\n" : "not ok 8\n";

# see if a modified @_ propagates
{
  package Foo;
  sub DESTROY	{ my $s = shift; print "ok $s->[0]\n"; }
  sub show	{ print "# @_\nnot ok $_[0][0]\n" if @_ != 5; }
  sub start	{ push @_, 1, "foo", {}; goto &show; }
  for (9..11)	{ start(bless([$_]), 'bar'); }
}

sub auto {
    goto &loadit;
}

sub AUTOLOAD { print @_ }

auto("ok 12\n");

$wherever = FINALE;
goto $wherever;
