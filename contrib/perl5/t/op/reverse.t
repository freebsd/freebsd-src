#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..4\n";

print "not " unless reverse("abc")    eq "cba";
print "ok 1\n";

$_ = "foobar";
print "not " unless reverse()         eq "raboof";
print "ok 2\n";

{
    my @a = ("foo", "bar");
    my @b = reverse @a;

    print "not " unless $b[0] eq $a[1] && $b[1] eq $a[0];
    print "ok 3\n";
}

{
    # Unicode.

    my $a = "\x{263A}\x{263A}x\x{263A}y\x{263A}";
    my $b = scalar reverse($a);
    my $c = scalar reverse($b);
    print "not " unless $a eq $c;
    print "ok 4\n";
}
