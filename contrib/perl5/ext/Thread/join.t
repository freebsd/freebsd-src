use Thread;
sub foo {
    print "In foo with args: @_\n";
    return (7, 8, 9);
}

print "Starting thread\n";
$t = new Thread \&foo, qw(foo bar baz);
print "Joining with $t\n";
@results = $t->join();
print "Joining returned ", scalar(@results), " values: @results\n";
