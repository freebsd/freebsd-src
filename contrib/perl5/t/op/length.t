#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..13\n";

print "not " unless length("")    == 0;
print "ok 1\n";

print "not " unless length("abc") == 3;
print "ok 2\n";

$_ = "foobar";
print "not " unless length()      == 6;
print "ok 3\n";

# Okay, so that wasn't very challenging.  Let's go Unicode.

{
    my $a = "\x{41}";

    print "not " unless length($a) == 1;
    print "ok 4\n";
    $test++;

    use bytes;
    print "not " unless $a eq "\x41" && length($a) == 1;
    print "ok 5\n";
    $test++;
}

{
    my $a = "\x{80}";
    
    print "not " unless length($a) == 1;
    print "ok 6\n";
    $test++;
    
    use bytes;
    print "not " unless $a eq "\xc2\x80" && length($a) == 2;
    print "ok 7\n";
    $test++;
}

{
    my $a = "\x{100}";
    
    print "not " unless length($a) == 1;
    print "ok 8\n";
    $test++;
    
    use bytes;
    print "not " unless $a eq "\xc4\x80" && length($a) == 2;
    print "ok 9\n";
    $test++;
}

{
    my $a = "\x{100}\x{80}";
    
    print "not " unless length($a) == 2;
    print "ok 10\n";
    $test++;
    
    use bytes;
    print "not " unless $a eq "\xc4\x80\xc2\x80" && length($a) == 4;
    print "ok 11\n";
    $test++;
}

{
    my $a = "\x{80}\x{100}";
    
    print "not " unless length($a) == 2;
    print "ok 12\n";
    $test++;
    
    use bytes;
    print "not " unless $a eq "\xc2\x80\xc4\x80" && length($a) == 4;
    print "ok 13\n";
    $test++;
}
