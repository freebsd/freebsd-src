# NOTE: this file tests how large files (>2GB) work with perlio (stdio/sfio).
# sysopen(), sysseek(), syswrite(), sysread() are tested in t/lib/syslfs.t.
# If you modify/add tests here, remember to update also t/lib/syslfs.t.

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
	# Don't bother if there are no quad offsets.
	require Config; import Config;
	if ($Config{lseeksize} < 8) {
		print "1..0 # Skip: no 64-bit file offsets\n";
		exit(0);
	}
}

use strict;

our @s;
our $fail;

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

my $explained;

sub explain {
    unless ($explained++) {
	print <<EOM;
#
# If the lfs (large file support: large meaning larger than two
# gigabytes) tests are skipped or fail, it may mean either that your
# process (or process group) is not allowed to write large files
# (resource limits) or that the file system (the network filesystem?)
# you are running the tests on doesn't let your user/group have large
# files (quota) or the filesystem simply doesn't support large files.
# You may even need to reconfigure your kernel.  (This is all very
# operating system and site-dependent.)
#
# Perl may still be able to support large files, once you have
# such a process, enough quota, and such a (file) system.
# It is just that the test failed now.
#
EOM
    }
    print "1..0 # Skip: @_\n" if @_;
}

print "# checking whether we have sparse files...\n";

# Known have-nots.
if ($^O eq 'MSWin32' || $^O eq 'VMS') {
    print "1..0 # Skip: no sparse files in $^O\n";
    bye();
}

# Known haves that have problems running this test
# (for example because they do not support sparse files, like UNICOS)
if ($^O eq 'unicos') {
    print "1..0 # Skip: no sparse files in $^0, unable to test large files\n";
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
	print "1..0 # Skip: no sparse files?\n";
	bye;
}

print "# we seem to have sparse files...\n";

# By now we better be sure that we do have sparse files:
# if we are not, the following will hog 5 gigabytes of disk.  Ooops.
# This may fail by producing some signal; run in a subprocess first for safety

$ENV{LC_ALL} = "C";

my $r = system '../perl', '-e', <<'EOF';
open(BIG, ">big");
seek(BIG, 5_000_000_000, 0);
print BIG "big";
exit 0;
EOF

open(BIG, ">big") or do { warn "open failed: $!\n"; bye };
binmode BIG;
if ($r or not seek(BIG, 5_000_000_000, $SEEK_SET)) {
    my $err = $r ? 'signal '.($r & 0x7f) : $!;
    explain("seeking past 2GB failed: $err");
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
	explain("writing past 2GB failed: process limits?");
    } elsif ($! =~ /quota/i) {
	explain("filesystem quota limits?");
    } else {
	explain("error: $!");
    }
    bye();
}

@s = stat("big");

print "# @s\n";

unless ($s[7] == 5_000_000_003) {
    explain("kernel/fs not configured to use large files?");
    bye();
}

sub fail () {
    print "not ";
    $fail++;
}

sub offset ($$) {
    my ($offset_will_be, $offset_want) = @_;
    my $offset_is = eval $offset_will_be;
    unless ($offset_is == $offset_want) {
        print "# bad offset $offset_is, want $offset_want\n";
	my ($offset_func) = ($offset_will_be =~ /^(\w+)/);
	if (unpack("L", pack("L", $offset_want)) == $offset_is) {
	    print "# 32-bit wraparound suspected in $offset_func() since\n";
	    print "# $offset_want cast into 32 bits equals $offset_is.\n";
	} elsif ($offset_want - unpack("L", pack("L", $offset_want)) - 1
	         == $offset_is) {
	    print "# 32-bit wraparound suspected in $offset_func() since\n";
	    printf "# %s - unpack('L', pack('L', %s)) - 1 equals %s.\n",
	        $offset_want,
	        $offset_want,
	        $offset_is;
        }
        fail;
    }
}

print "1..17\n";

$fail = 0;

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

offset('tell(BIG)', 4_500_000_000);
print "ok 6\n";

fail unless seek(BIG, 1, $SEEK_CUR);
print "ok 7\n";

# If you get 205_032_705 from here it means that
# your tell() is returning 32-bit values since (I32)4_500_000_001
# is exactly 205_032_705.
offset('tell(BIG)', 4_500_000_001);
print "ok 8\n";

fail unless seek(BIG, -1, $SEEK_CUR);
print "ok 9\n";

offset('tell(BIG)', 4_500_000_000);
print "ok 10\n";

fail unless seek(BIG, -3, $SEEK_END);
print "ok 11\n";

offset('tell(BIG)', 5_000_000_000);
print "ok 12\n";

my $big;

fail unless read(BIG, $big, 3) == 3;
print "ok 13\n";

fail unless $big eq "big";
print "ok 14\n";

# 705_032_704 = (I32)5_000_000_000
# See that we don't have "big" in the 705_... spot:
# that would mean that we have a wraparound.
fail unless seek(BIG, 705_032_704, $SEEK_SET);
print "ok 15\n";

my $zero;

fail unless read(BIG, $zero, 3) == 3;
print "ok 16\n";

fail unless $zero eq "\0\0\0";
print "ok 17\n";

explain() if $fail;

bye(); # does the necessary cleanup

END {
   unlink "big"; # be paranoid about leaving 5 gig files lying around
}

# eof
