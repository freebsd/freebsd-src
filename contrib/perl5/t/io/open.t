#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}    

# $RCSfile$    
$|  = 1;
use warnings;
$Is_VMS = $^O eq 'VMS';
$Is_Dos = $^O eq 'dos';

print "1..66\n";

my $test = 1;

sub ok { print "ok $test\n"; $test++ }

# my $file tests

# 1..9
{
    unlink("afile") if -f "afile";     
    print "$!\nnot " unless open(my $f,"+>afile");
    ok;
    binmode $f;
    print "not " unless -f "afile";     
    ok;
    print "not " unless print $f "SomeData\n";
    ok;
    print "not " unless tell($f) == 9;
    ok;
    print "not " unless seek($f,0,0);
    ok;
    $b = <$f>;
    print "not " unless $b eq "SomeData\n";
    ok;
    print "not " unless -f $f;     
    ok;
    eval  { die "Message" };   
    # warn $@;
    print "not " unless $@ =~ /<\$f> line 1/;
    ok;
    print "not " unless close($f);
    ok;
    unlink("afile");     
}

# 10..12
{
    print "# \$!='$!'\nnot " unless open(my $f,'>', 'afile');
    ok;
    print $f "a row\n";
    print "not " unless close($f);
    ok;
    print "not " unless -s 'afile' < 10;
    ok;
}

# 13..15
{
    print "# \$!='$!'\nnot " unless open(my $f,'>>', 'afile');
    ok;
    print $f "a row\n";
    print "not " unless close($f);
    ok;
    print "not " unless -s 'afile' > 10;
    ok;
}

# 16..18
{
    print "# \$!='$!'\nnot " unless open(my $f, '<', 'afile');
    ok;
    @rows = <$f>;
    print "not " unless @rows == 2;
    ok;
    print "not " unless close($f);
    ok;
}

# 19..23
{
    print "not " unless -s 'afile' < 20;
    ok;
    print "# \$!='$!'\nnot " unless open(my $f, '+<', 'afile');
    ok;
    @rows = <$f>;
    print "not " unless @rows == 2;
    ok;
    seek $f, 0, 1;
    print $f "yet another row\n";
    print "not " unless close($f);
    ok;
    print "not " unless -s 'afile' > 20;
    ok;

    unlink("afile");     
}

# 24..26
if ($Is_VMS) {
    for (24..26) { print "ok $_ # skipped: not Unix fork\n"; $test++;}
}
else {
    print "# \$!='$!'\nnot " unless open(my $f, '-|', <<'EOC');
    ./perl -e "print qq(a row\n); print qq(another row\n)"
EOC
    ok;
    @rows = <$f>;
    print "not " unless @rows == 2;
    ok;
    print "not " unless close($f);
    ok;
}

# 27..30
if ($Is_VMS) {
    for (27..30) { print "ok $_ # skipped: not Unix fork\n"; $test++;}
}
else {
    print "# \$!='$!'\nnot " unless open(my $f, '|-', <<'EOC');
    ./perl -pe "s/^not //"
EOC
    ok;
    @rows = <$f>;
    print $f "not ok $test\n"; $test++;
    print $f "not ok $test\n"; $test++;
    print "#\nnot " unless close($f);
    sleep 1;
    ok;
}

# 31..32
eval <<'EOE' and print "not ";
open my $f, '<&', 'afile';
1;
EOE
ok;
$@ =~ /Unknown open\(\) mode \'<&\'/ or print "not ";
ok;

# local $file tests

# 33..41
{
    unlink("afile") if -f "afile";     
    print "$!\nnot " unless open(local $f,"+>afile");
    ok;
    binmode $f;
    print "not " unless -f "afile";     
    ok;
    print "not " unless print $f "SomeData\n";
    ok;
    print "not " unless tell($f) == 9;
    ok;
    print "not " unless seek($f,0,0);
    ok;
    $b = <$f>;
    print "not " unless $b eq "SomeData\n";
    ok;
    print "not " unless -f $f;     
    ok;
    eval  { die "Message" };   
    # warn $@;
    print "not " unless $@ =~ /<\$f> line 1/;
    ok;
    print "not " unless close($f);
    ok;
    unlink("afile");     
}

# 42..44
{
    print "# \$!='$!'\nnot " unless open(local $f,'>', 'afile');
    ok;
    print $f "a row\n";
    print "not " unless close($f);
    ok;
    print "not " unless -s 'afile' < 10;
    ok;
}

# 45..47
{
    print "# \$!='$!'\nnot " unless open(local $f,'>>', 'afile');
    ok;
    print $f "a row\n";
    print "not " unless close($f);
    ok;
    print "not " unless -s 'afile' > 10;
    ok;
}

# 48..50
{
    print "# \$!='$!'\nnot " unless open(local $f, '<', 'afile');
    ok;
    @rows = <$f>;
    print "not " unless @rows == 2;
    ok;
    print "not " unless close($f);
    ok;
}

# 51..55
{
    print "not " unless -s 'afile' < 20;
    ok;
    print "# \$!='$!'\nnot " unless open(local $f, '+<', 'afile');
    ok;
    @rows = <$f>;
    print "not " unless @rows == 2;
    ok;
    seek $f, 0, 1;
    print $f "yet another row\n";
    print "not " unless close($f);
    ok;
    print "not " unless -s 'afile' > 20;
    ok;

    unlink("afile");     
}

# 56..58
if ($Is_VMS) {
    for (56..58) { print "ok $_ # skipped: not Unix fork\n"; $test++;}
}
else {
    print "# \$!='$!'\nnot " unless open(local $f, '-|', <<'EOC');
    ./perl -e "print qq(a row\n); print qq(another row\n)"
EOC
    ok;
    @rows = <$f>;
    print "not " unless @rows == 2;
    ok;
    print "not " unless close($f);
    ok;
}

# 59..62
if ($Is_VMS) {
    for (59..62) { print "ok $_ # skipped: not Unix fork\n"; $test++;}
}
else {
    print "# \$!='$!'\nnot " unless open(local $f, '|-', <<'EOC');
    ./perl -pe "s/^not //"
EOC
    ok;
    @rows = <$f>;
    print $f "not ok $test\n"; $test++;
    print $f "not ok $test\n"; $test++;
    print "#\nnot " unless close($f);
    sleep 1;
    ok;
}

# 63..64
eval <<'EOE' and print "not ";
open local $f, '<&', 'afile';
1;
EOE
ok;
$@ =~ /Unknown open\(\) mode \'<&\'/ or print "not ";
ok;

# 65..66
{
    local *F;
    for (1..2) {
        if ($Is_Dos) {
        open(F, "echo \\#foo|") or print "not ";
        } else {
            open(F, "echo #foo|") or print "not ";
        }
	print <F>;
	close F;
    }
    ok;
    for (1..2) {
        if ($Is_Dos) {
	open(F, "-|", "echo \\#foo") or print "not ";
        } else {
            open(F, "-|", "echo #foo") or print "not ";
        }
	print <F>;
	close F;
    }
    ok;
}
