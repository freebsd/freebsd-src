#!./perl

# Regression tests for attrs.pm and the C<sub x : attrs> syntax.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    eval 'require attrs; 1' or do {
	print "1..0\n";
	exit 0;
    }
}

sub NTESTS () ;

my $test, $ntests;
BEGIN {$ntests=0}
$test=0;
my $failed = 0;

print "1..".NTESTS."\n";

eval 'sub t1 ($) { use attrs "locked"; $_[0]++ }';
(print "not "), $failed=1 if $@;
print "ok ",++$test,"\n";
BEGIN {++$ntests}

eval 'sub t2 { use attrs "locked"; $_[0]++ }';
(print "not "), $failed=1 if $@;
print "ok ",++$test,"\n";
BEGIN {++$ntests}

eval 'sub t3 ($) : locked ;';
(print "not "), $failed=1 if $@;
print "ok ",++$test,"\n";
BEGIN {++$ntests}

eval 'sub t4 : locked ;';
(print "not "), $failed=1 if $@;
print "ok ",++$test,"\n";
BEGIN {++$ntests}

my $anon1;
eval '$anon1 = sub ($) { use attrs qw(locked method); $_[0]++ }';
(print "not "), $failed=1 if $@;
print "ok ",++$test,"\n";
BEGIN {++$ntests}

my $anon2;
eval '$anon2 = sub { use attrs qw(locked method); $_[0]++ }';
(print "not "), $failed=1 if $@;
print "ok ",++$test,"\n";
BEGIN {++$ntests}

my $anon3;
eval '$anon3 = sub { use attrs "method"; $_[0]->[1] }';
(print "not "), $failed=1 if $@;
print "ok ",++$test,"\n";
BEGIN {++$ntests}

my @attrs = attrs::get($anon3 ? $anon3 : \&ns);
(print "not "), $failed=1 unless "@attrs" eq "method";
print "ok ",++$test,"\n";
BEGIN {++$ntests}

@attrs = sort +attrs::get($anon2 ? $anon2 : \&ns);
(print "not "), $failed=1 unless "@attrs" eq "locked method";
print "ok ",++$test,"\n";
BEGIN {++$ntests}

@attrs = sort +attrs::get($anon1 ? $anon1 : \&ns);
(print "not "), $failed=1 unless "@attrs" eq "locked method";
print "ok ",++$test,"\n";
BEGIN {++$ntests}

eval 'sub e1 ($) : plugh ;';
unless ($@ && $@ =~ m/^Invalid CODE attribute: ["']?plugh["']? at/) {
    my $x = $@;
    $x =~ s/\n.*\z//s;
    print "# $x\n";
    print "not ";
    $failed = 1;
}
print "ok ",++$test,"\n";
BEGIN {++$ntests}

eval 'sub e2 ($) : plugh(0,0) xyzzy ;';
unless ($@ && $@ =~ m/^Invalid CODE attributes: ["']?plugh\(0,0\)["']? /) {
    my $x = $@;
    $x =~ s/\n.*\z//s;
    print "# $x\n";
    print "not ";
    $failed = 1;
}
print "ok ",++$test,"\n";
BEGIN {++$ntests}

eval 'sub e3 ($) : plugh(0,0 xyzzy ;';
unless ($@ && $@ =~ m/Unterminated attribute parameter in attribute list at/) {
    my $x = $@;
    $x =~ s/\n.*\z//s;
    print "# $x\n";
    print "not ";
    $failed = 1;
}
print "ok ",++$test,"\n";
BEGIN {++$ntests}

eval 'sub e4 ($) : plugh + xyzzy ;';
unless ($@ && $@ =~ m/Invalid separator character '[+]' in attribute list at/) {
    my $x = $@;
    $x =~ s/\n.*\z//s;
    print "# $x\n";
    print "not ";
    $failed = 1;
}
print "ok ",++$test,"\n";
BEGIN {++$ntests}

{
    my $w = "" ;
    local $SIG{__WARN__} = sub {$w = @_[0]} ;
    eval 'sub w1 ($) { use warnings "deprecated"; use attrs "locked"; $_[0]++ }';
    (print "not "), $failed=1 if $@;
    print "ok ",++$test,"\n";
    BEGIN {++$ntests}
    (print "not "), $failed=1 
	if $w !~ /^pragma "attrs" is deprecated, use "sub NAME : ATTRS" instead at/;
    print "ok ",++$test,"\n";
    BEGIN {++$ntests}
}


# Other tests should be added above this line

sub NTESTS () { $ntests }

exit $failed;
