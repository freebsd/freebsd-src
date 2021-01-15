#!/usr/bin/perl
use strict;
use warnings;
use IPC::Open2;
my $pid;

open(my $lsfiles, "-|", "git ls-files -s") or die "fork lsfiles: $!";

while (<$lsfiles>) {
    if ($_ =~ m/^120000 ([0-9a-f]{40}) (.*)\t(.*)/) {
	my ($obj, $stage, $path) = ($1,$2,$3);
	if (!defined $pid) {
	    $pid = open2(*Rderef, *Wderef, "git cat-file --batch-check='deref-ok %(objectname)' --follow-symlinks")
		or die "open git cat-file: $!";
	}
	print Wderef "$ENV{GIT_COMMIT}:$path\n" or die "write Wderef: $!";
	my $deref = <Rderef>;
	if ($deref =~ m/^deref-ok ([0-9a-f]{40})$/) {
	    $_ = "100644 $1 $stage\t$path\n"
	} elsif ($deref =~ /^dangling /) {
	    # Skip next line
	    my $dummy = <Rderef>;
	} else {
	    die "Failed to parse symlink $ENV{GIT_COMMIT}:$path $deref";
	}
    }

    my $m = 0;

    # Keep the copyright. Also ensures we never have a completely empty commit.
    $m++ if m/\tCOPYING$/;

    # A few architectures have dts files at non standard paths. Massage those into
    # a standard arch/ARCH/boot/dts first.

    # symlink: arch/microblaze/boot/dts/system.dts -> ../../platform/generic/system.dts
    next if m,\tarch/microblaze/boot/dts/system.dts$,;
    $m++ if s,\tarch/microblaze/platform/generic/(system.dts)$,\tarch/microblaze/boot/dts/$1,;

    # arch/mips/lantiq/dts/easy50712.dts
    # arch/mips/lantiq/dts/danube.dtsi
    # arch/mips/netlogic/dts/xlp_evp.dts
    # arch/mips/ralink/dts/rt3050.dtsi
    # arch/mips/ralink/dts/rt3052_eval.dts
    $m++ if s,\tarch/mips/([^/]*)/dts/(.*\.dts.?)$,\tarch/mips/boot/dts/$2,;

    # arch/mips/cavium-octeon/octeon_68xx.dts
    # arch/mips/cavium-octeon/octeon_3xxx.dts
    # arch/mips/mti-sead3/sead3.dts
    $m++ if s,\tarch/mips/([^/]*)/([^/]*\.dts.?)$,\tarch/mips/boot/dts/$2,;

    # arch/x86/platform/ce4100/falconfalls.dts
    $m++ if s,\tarch/x86/platform/ce4100/falconfalls.dts,\tarch/x86/boot/dts/falconfalls.dts,;

    # test cases
    $m++ if s,\tdrivers/of/testcase-data/,\ttestcase-data/,;

    # Now rewrite generic DTS paths
    $m++ if s,\tarch/([^/]*)/boot/dts/(.*\.dts.?)$,\tsrc/$1/$2,;
    $m++ if s,\tarch/([^/]*)/boot/dts/(.*\.h)$,\tsrc/$1/$2,;

    # Also rewrite the DTS include paths for dtc+cpp support
    $m++ if s,\tarch/([^/]*)/include/dts/,\tsrc/$1/include/,;
    $m++ if s,\tinclude/dt-bindings/,\tinclude/dt-bindings/,;

    # Rewrite the bindings subdirectory
    $m++ if s,\tDocumentation/devicetree/bindings/,\tBindings/,;

    print if $m > 0;
}
kill $pid if $pid;
exit 0;
