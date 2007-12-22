#!/usr/bin/env perl -w
#
# Copyright (c) 2005, 2006 Marcel Moolenaar
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
# OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
# IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
# NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
# THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
# $FreeBSD$

my $srcdir = `dirname $0`;
chomp $srcdir;

my $cmd = "/tmp/gctl-$$";
my $out = "$cmd.out";
my $disk = "/tmp/disk-$$";
my $mntpt = "/tmp/mount-$$";

my %steps = (
    "000" => "gctl",
    "001" => "gctl verb=bogus",
    "010" => "gctl verb=create",
    "011" => "gctl verb=create provider=bogus",
    "020" => "mdcfg create pristine",
    "021" => "gctl verb=create provider=%dev% entries=-1",
    "022" => "gctl verb=create provider=%dev% entries=128",
    "023" => "gctl verb=create provider=%dev%",
    "024" => "gctl verb=modify geom=%dev%",
    "025" => "conf",
    "030" => "gctl verb=add",
    "031" => "gctl verb=add geom=bogus",
    "032" => "gctl verb=add geom=%dev%",
    "033" => "gctl verb=add geom=%dev% type=bogus",
    "034" => "gctl verb=add geom=%dev% type=ed0101b0-2a71-11da-ba81-003048416ace",
    "035" => "gctl verb=add geom=%dev% type=ed0101b0-2a71-11da-ba81-003048416ace start=1",
    "036" => "gctl verb=add geom=%dev% type=ed0101b0-2a71-11da-ba81-003048416ace start=34",
    "037" => "gctl verb=add geom=%dev% type=ed0101b0-2a71-11da-ba81-003048416ace start=34 end=12345678",
    "038" => "gctl verb=add geom=%dev% type=ed0101b0-2a71-11da-ba81-003048416ace start=162 end=417 entry=129",
    "039" => "gctl verb=add geom=%dev% type=ed0101b0-2a71-11da-ba81-003048416ace start=162 end=417 entry:8=5",
    "040" => "gctl verb=add geom=%dev% type=83d34ed5-c4ff-11da-b65b-000347c5d7f3 start=34 end=161 entry=5",
    "041" => "gctl verb=add geom=%dev% type=83d34ed5-c4ff-11da-b65b-000347c5d7f3 start=34 end=546",
    "042" => "gctl verb=add geom=%dev% type=83d34ed5-c4ff-11da-b65b-000347c5d7f3 start=162 end=417",
    "043" => "gctl verb=add geom=%dev% type=83d34ed5-c4ff-11da-b65b-000347c5d7f3 start=100 end=300",
    "044" => "gctl verb=add geom=%dev% type=83d34ed5-c4ff-11da-b65b-000347c5d7f3 start=300 end=500",
    "045" => "gctl verb=add geom=%dev% type=83d34ed5-c4ff-11da-b65b-000347c5d7f3 start=34 end=161 entry:8",
    "046" => "gctl verb=add geom=%dev% type=d2bd4509-c4ff-11da-b4cc-00306e39b62f start=418 end=546 entry:8",
    "047" => "conf",
    "050" => "gctl verb=remove geom=%dev% entry=5",
    "051" => "gctl verb=remove geom=%dev% entry=2",
    "052" => "gctl verb=remove geom=%dev% entry=1",
    "053" => "gctl verb=remove geom=%dev% entry=1",
    "054" => "conf",
    "060" => "gctl verb=add geom=%dev% type=516e7cb6-6ecf-11d6-8ff8-00022d09712b start=34 end=546 entry:8=1",
    "061" => "mount %dev%p1",
    "062" => "gctl verb=remove geom=%dev% entry=1",
    "063" => "umount %dev%p1",
    "064" => "gctl verb=remove geom=%dev% entry=1",
    "065" => "conf",
    "100" => "mdcfg destroy",
    "110" => "mdcfg create corrupted",
    "111" => "gctl verb=add geom=%dev%",
    "120" => "mdcfg destroy",
);

my %result = (
    "000" => "FAIL Verb missing",
    "001" => "FAIL 22 verb 'bogus'",
    "010" => "FAIL 87 provider",
    "011" => "FAIL 22 provider 'bogus'",
    "020" => "",
    "021" => "FAIL 22 entries -1",
    "022" => "PASS",
    "023" => "FAIL 17 geom '%dev%'",
    "024" => "FAIL 87 entry",
    "025" => "b1856477950e5786898c8f01361196cf",
    "030" => "FAIL 87 geom",
    "031" => "FAIL 22 geom 'bogus'",
    "032" => "FAIL 87 type",
    "033" => "FAIL 22 type 'bogus'",
    "034" => "FAIL 87 start",
    "035" => "FAIL 22 start 1",
    "036" => "FAIL 87 end",
    "037" => "FAIL 22 end 12345678",
    "038" => "FAIL 22 entry 129",
    "039" => "PASS entry=5",
    "040" => "FAIL 17 entry 5",
    "041" => "FAIL 28 start/end 34/546",
    "042" => "FAIL 28 start/end 162/417",
    "043" => "FAIL 28 start/end 100/300",
    "044" => "FAIL 28 start/end 300/500",
    "045" => "PASS entry=1",
    "046" => "PASS entry=2",
    "047" => "50783a39eecfc62a29db24381e12b9d8",
    "050" => "PASS",
    "051" => "PASS",
    "052" => "PASS",
    "053" => "FAIL 2 entry 1",
    "054" => "b1856477950e5786898c8f01361196cf",
    "060" => "PASS",
    "061" => "PASS",
    "062" => "FAIL 16",
    "063" => "PASS",
    "064" => "PASS",
    "065" => "b1856477950e5786898c8f01361196cf",
    "100" => "",
    "110" => "",
    "111" => "FAIL 6 geom '%dev%'",
    "120" => "",
);

my $verbose = "";
if (exists $ENV{'TEST_VERBOSE'}) {
    $verbose = "-v";
}

# Compile the driver...
my $st = system("cc -o $cmd -g $srcdir/test.c -lgeom");
if ($st != 0) {
    print "1..0 # SKIP error compiling test.c\n";
    exit 0;
}

# Make sure we have permission to use gctl...
if (`$cmd` =~ "^FAIL Permission denied") {
    print "1..0 # SKIP not enough permission\n";
    unlink $cmd;
    exit 0;
}

$count = keys (%steps);
print "1..$count\n";

my $nr = 1;
my $dev = "n/a";
foreach my $key (sort keys %steps) {
    my ($action, $args) = split(/ /, $steps{$key}, 2);
    my $res = $result{$key};
    $args = "" if (not defined $args);
    $args =~ s/%dev%/$dev/g;
    $res =~ s/%dev%/$dev/g;

    if ($action =~ "^gctl") {
	system("$cmd $verbose $args | tee $out 2>&1");
	$st = `tail -1 $out`;
	if ($st =~ "^$res") {
	    print "ok $nr \# gctl($key)\n";
	} else {
	    print "not ok $nr \# gctl($key) - $st\n";
	}
	unlink $out;
    } elsif ($action =~ "^mdcfg") {
	if ($args =~ "^create") {
	    system("dd if=/dev/zero of=$disk count=1024 2>&1");
	    if ($args =~ "corrupted") {
		system("gpt create -p $disk");
	    }
	    $dev = `mdconfig -a -t vnode -f $disk`;
	    chomp $dev;
	} elsif ($args =~ "^destroy") {
	    $dev =~ s/md/-u /g;
	    system("mdconfig -d $dev");
	    unlink $disk;
	    $dev = "n/a";
	}
	print "ok $nr \# mdcfg($key)\n";
    } elsif ($action =~ "^conf") {
	system("sysctl -b kern.geom.conftxt | grep -a $dev | sed -e s:$disk:DISK:g -e s:$dev:DEV:g | sort | md5 -p | tee $out 2>&1");
	$st = `tail -1 $out`;
	if ($st =~ "^$res") {
	    print "ok $nr \# conf($key)\n";
	} else {
	    print "not ok $nr \# conf($key) - $st\n";
	}
	unlink $out;
    } elsif ($action =~ "^mount") {
	    system("mkdir $mntpt-$args");
	    system("newfs $args");
	    system("mount -t ufs /dev/$args $mntpt-$args");
	    print "ok $nr \# mount($key)\n";
    } elsif ($action =~ "^umount") {
	    system("umount $mntpt-$args");
	    system("rmdir $mntpt-$args");
	    print "ok $nr \# umount($key)\n";
    }
    $nr += 1;
}

unlink $cmd;
exit 0;
