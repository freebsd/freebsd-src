# NOTE: this file tests how large files (>2GB) work with perlio (stdio/sfio).
# sysopen(), sysseek(), syswrite(), sysread() are tested in t/lib/syslfs.t.
# If you modify/add tests here, remember to update also t/lib/syslfs.t.

BEGIN {
	chdir 't' if -d 't';
	unshift @INC, '../lib';
	# Don't bother if there are no quad offsets.
	require Config; import Config;
	if ($Config{lseeksize} < 8) {
		print "1..0\n# no 64-bit file offsets\n";
		exit(0);
	}
}

sub zap {
    close(BIG);
    unlink("big");
    unlink("big1");
    unlink("big2");
}

sub bye {
    zap();	
    exit(0);
}

sub explain {
    print <<EOM;
#
# If the lfs (large file support: large meaning larger than two gigabytes)
# tests are skipped or fail, it may mean either that your process
# (or process group) is not allowed to write large files (resource
# limits) or that the file system you are running the tests on doesn't
# let your user/group have large files (quota) or the filesystem simply
# doesn't support large files.  You may even need to reconfigure your kernel.
# (This is all very operating system and site-dependent.)
#
# Perl may still be able to support large files, once you have
# such a process, enough quota, and such a (file) system.
#
EOM
}

print "# checking whether we have sparse files...\n";

# Known have-nots.
if ($^O eq 'win32' || $^O eq 'vms') {
    print "1..0\n# no sparse files (because this is $^O) \n";
    bye();
}

# Known haves that have problems running this test
# (for example because they do not support sparse files, like UNICOS)
if ($^O eq 'unicos') {
    print "1..0\n# large files known to work but unable to test them here ($^O)\n";
    bye();
}

# Then try to heuristically deduce whether we have sparse files.

# Let's not depend on Fcntl or any other extension.

my ($SEEK_SET, $SEEK_CUR, $SEEK_END) = (0, 1, 2);

# We'll start off by creating a one megabyte file which has
# only three "true" bytes.  If we have sparseness, we should
# consume less blocks than one megabyte (assuming nobody has
# one megabyte blocks...)

open(BIG, ">big1") or
    do { warn "open big1 failed: $!\n"; bye };
binmode(BIG) or
    do { warn "binmode big1 failed: $!\n"; bye };
seek(BIG, 1_000_000, $SEEK_SET) or
    do { warn "seek big1 failed: $!\n"; bye };
print BIG "big" or
    do { warn "print big1 failed: $!\n"; bye };
close(BIG) or
    do { warn "close big1 failed: $!\n"; bye };

my @s1 = stat("big1");

print "# s1 = @s1\n";

open(BIG, ">big2") or
    do { warn "open big2 failed: $!\n"; bye };
binmode(BIG) or
    do { warn "binmode big2 failed: $!\n"; bye };
seek(BIG, 2_000_000, $SEEK_SET) or
    do { warn "seek big2 failed; $!\n"; bye };
print BIG "big" or
    do { warn "print big2 failed; $!\n"; bye };
close(BIG) or
    do { warn "close big2 failed; $!\n"; bye };

my @s2 = stat("big2");

print "# s2 = @s2\n";

zap();

unless ($s1[7] == 1_000_003 && $s2[7] == 2_000_003 &&
	$s1[11] == $s2[11] && $s1[12] == $s2[12]) {
	print "1..0\n#no sparse files?\n";
	bye;
}

print "# we seem to have sparse files...\n";

# By now we better be sure that we do have sparse files:
# if we are not, the following will hog 5 gigabytes of disk.  Ooops.

$ENV{LC_ALL} = "C";

open(BIG, ">big") or do { warn "open failed: $!\n"; bye };
binmode BIG;
unless (seek(BIG, 5_000_000_000, $SEEK_SET)) {
    print "1..0\n# seeking past 2GB failed: $!\n";
    explain();
    bye();
}

# Either the print or (more likely, thanks to buffering) the close will
# fail if there are are filesize limitations (process or fs).
my $print = print BIG "big";
print "# print failed: $!\n" unless $print;
my $close = close BIG;
print "# close failed: $!\n" unless $close;
unless ($print && $close) {
    if ($! =~/too large/i) {
	print "1..0\n# writing past 2GB failed: process limits?\n";
    } elsif ($! =~ /quota/i) {
	print "1..0\n# filesystem quota limits?\n";
    }
    explain();
    bye();
}

@s = stat("big");

print "# @s\n";

unless ($s[7] == 5_000_000_003) {
    print "1..0\n# not configured to use large files?\n";
    explain();
    bye();
}

sub fail () {
    print "not ";
    $fail++;
}

print "1..17\n";

my $fail = 0;

fail unless $s[7] == 5_000_000_003;	# exercizes pp_stat
print "ok 1\n";

fail unless -s "big" == 5_000_000_003;	# exercizes pp_ftsize
print "ok 2\n";

fail unless -e "big";
print "ok 3\n";

fail unless -f "big";
print "ok 4\n";

open(BIG, "big") or do { warn "open failed: $!\n"; bye };
binmode BIG;

fail unless seek(BIG, 4_500_000_000, $SEEK_SET);
print "ok 5\n";

fail unless tell(BIG) == 4_500_000_000;
print "ok 6\n";

fail unless seek(BIG, 1, $SEEK_CUR);
print "ok 7\n";

fail unless tell(BIG) == 4_500_000_001;
print "ok 8\n";

fail unless seek(BIG, -1, $SEEK_CUR);
print "ok 9\n";

fail unless tell(BIG) == 4_500_000_000;
print "ok 10\n";

fail unless seek(BIG, -3, $SEEK_END);
print "ok 11\n";

fail unless tell(BIG) == 5_000_000_000;
print "ok 12\n";

my $big;

fail unless read(BIG, $big, 3) == 3;
print "ok 13\n";

fail unless $big eq "big";
print "ok 14\n";

# 705_032_704 = (I32)5_000_000_000
fail unless seek(BIG, 705_032_704, $SEEK_SET);
print "ok 15\n";

my $zero;

fail unless read(BIG, $zero, 3) == 3;
print "ok 16\n";

fail unless $zero eq "\0\0\0";
print "ok 17\n";

explain if $fail;

bye(); # does the necessary cleanup

END {
   unlink "big"; # be paranoid about leaving 5 gig files lying around
}

# eof
