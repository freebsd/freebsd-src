use Thread 'async';

$t = async {
    print "here\n";
    die "success";
    print "shouldn't get here\n";
};

sleep 1;
print "joining...\n";
eval { @r = $t->join; };
if ($@) {
    print "thread died with message: $@";
} else {
    print "thread failed to die successfully\n";
}
