# NOTE: this file tests how large files (>2GB) work with raw system IO.
# stdio: open(), tell(), seek(), print(), read() is tested in t/op/lfs.t.
# If you modify/add tests here, remember to update also t/op/lfs.t.

BEGIN {
	chdir 't' if -d 't';
	@INC = '../lib';
	require Config; import Config;
	# Don't bother if there are no quad offsets.
	if ($Config{lseeksize} < 8) {
		print "1..0 # Skip: no 64-bit file offsets\n";
		exit(0);
	}
	require Fcntl; import Fcntl qw(/^O_/ /^SEEK_/);
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

# Then try heuristically to deduce whether we have sparse files.

# We'll start off by creating a one megabyte file which has
# only three "true" bytes.  If we have sparseness, we should
# consume less blocks than one megabyte (assuming nobody has
# one megabyte blocks...)

sysopen(BIG, "big1", O_WRONLY|O_CREAT|O_TRUNC) or
    do { warn "sysopen big1 failed: $!\n"; bye };
sysseek(BIG, 1_000_000, SEEK_SET) or
    do { warn "sysseek big1 failed: $!\n"; bye };
syswrite(BIG, "big") or
    do { warn "syswrite big1 failed; $!\n"; bye };
close(BIG) or
    do { warn "close big1 failed: $!\n"; bye };

my @s1 = stat("big1");

print "# s1 = @s1\n";

sysopen(BIG, "big2", O_WRONLY|O_CREAT|O_TRUNC) or
    do { warn "sysopen big2 failed: $!\n"; bye };
sysseek(BIG, 2_000_000, SEEK_SET) or
    do { warn "sysseek big2 failed: $!\n"; bye };
syswrite(BIG, "big") or
    do { warn "syswrite big2 failed; $!\n"; bye };
close(BIG) or
    do { warn "close big2 failed: $!\n"; bye };

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

my $r = system '../perl', '-I../lib', '-e', <<'EOF';
use Fcntl qw(/^O_/ /^SEEK_/);
sysopen(BIG, "big", O_WRONLY|O_CREAT|O_TRUNC) or die $!;
my $sysseek = sysseek(BIG, 5_000_000_000, SEEK_SET);
my $syswrite = syswrite(BIG, "big");
exit 0;
EOF

sysopen(BIG, "big", O_WRONLY|O_CREAT|O_TRUNC) or
	do { warn "sysopen 'big' failed: $!\n"; bye };
my $sysseek = sysseek(BIG, 5_000_000_000, SEEK_SET);
unless (! $r && defined $sysseek && $sysseek == 5_000_000_000) {
    $sysseek = 'undef' unless defined $sysseek;
    explain("seeking past 2GB failed: ",
	    $r ? 'signal '.($r & 0x7f) : "$! (sysseek returned $sysseek)");
    bye();
}

# The syswrite will fail if there are are filesize limitations (process or fs).
my $syswrite = syswrite(BIG, "big");
print "# syswrite failed: $! (syswrite returned ",
      defined $syswrite ? $syswrite : 'undef', ")\n"
    unless defined $syswrite && $syswrite == 3;
my $close     = close BIG;
print "# close failed: $!\n" unless $close;
unless($syswrite && $close) {
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

sysopen(BIG, "big", O_RDONLY) or do { warn "sysopen failed: $!\n"; bye };

offset('sysseek(BIG, 4_500_000_000, SEEK_SET)', 4_500_000_000);
print "ok 5\n";

offset('sysseek(BIG, 0, SEEK_CUR)', 4_500_000_000);
print "ok 6\n";

offset('sysseek(BIG, 1, SEEK_CUR)', 4_500_000_001);
print "ok 7\n";

offset('sysseek(BIG, 0, SEEK_CUR)', 4_500_000_001);
print "ok 8\n";

offset('sysseek(BIG, -1, SEEK_CUR)', 4_500_000_000);
print "ok 9\n";

offset('sysseek(BIG, 0, SEEK_CUR)', 4_500_000_000);
print "ok 10\n";

offset('sysseek(BIG, -3, SEEK_END)', 5_000_000_000);
print "ok 11\n";

offset('sysseek(BIG, 0, SEEK_CUR)', 5_000_000_000);
print "ok 12\n";

my $big;

fail unless sysread(BIG, $big, 3) == 3;
print "ok 13\n";

fail unless $big eq "big";
print "ok 14\n";

# 705_032_704 = (I32)5_000_000_000
# See that we don't have "big" in the 705_... spot:
# that would mean that we have a wraparound.
fail unless sysseek(BIG, 705_032_704, SEEK_SET);
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
