use Thread;
sub foo {
    print "In foo with args: @_\n";
    return (7, 8, 9);
}

print "Starting thread\n";
$t = new Thread \&foo, qw(foo bar baz);
sleep 2;
print "Joining with $t\n";
@results = $t->join();
print "Joining returned @results\n";
