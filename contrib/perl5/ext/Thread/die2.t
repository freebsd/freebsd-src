use Thread 'async';

$t = async {
    sleep 1;
    print "here\n";
    die "success if preceded by 'thread died...'";
    print "shouldn't get here\n";
};

print "joining...\n";
@r = eval { $t->join; };
if ($@) {
    print "thread died with message: $@";
} else {
    print "thread failed to die successfully\n";
}
