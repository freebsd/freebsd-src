#!/usr/bin/perl -w

# Copyright (c) 2000
#  Paul Richards. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer,
#    verbatim and that no modifications are made prior to this
#    point in the file.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY PAUL RICHARDS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL PAUL RICHARDS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# $FreeBSD$
#/

use strict;

use File::Basename;
use Getopt::Std;

my $PKG_DB = "/var/db/pkg";
my $PKG_DEP_FILE = "+REQUIRED_BY";

my $PKG_ADD = "/usr/sbin/pkg_add";
my $PKG_CREATE = "/usr/sbin/pkg_create";
my $PKG_DELETE = "/usr/sbin/pkg_delete -f";
my $PKG_INFO = "/usr/sbin/pkg_info -Ia";

sub error ($) {
	my ($error) = @_;

	print STDERR $error, "\n";
}

sub get_version($) {
	my ($pkg) = @_;

	$pkg =~ /(.+)-([0-9\._,]+)/;
	if (! $2) {
		return($pkg, "");
	} else {
		return ($1, $2);
	}
}

sub get_requires($$) {
	my ($pkg, $requires) = @_;

	my $file = "$PKG_DB/$pkg/$PKG_DEP_FILE";

	if (! -f $file) {
		# Not all packages have dependencies
		return 1;
	}

	if (! open(REQUIRES, "< $file")) {
		error("Can't open $file, $!");
		return 0;
	}

	while (<REQUIRES>) {
		chomp $_;
		$$requires{$_} = 1;
	}

	close(REQUIRES) || warn("Can't close $file, $!");

	return 1;
}

sub put_requires($$) {
	my ($pkg, $requires) = @_;

	my $file = "$PKG_DB/$pkg/$PKG_DEP_FILE";

	if (! open(REQUIRES, "> $file")) {
		error("Can't open $file, $!");
		return 0;
	}

	my $req;
	for $req (keys %$requires) {
		print REQUIRES $req, "\n";
	}

	if (! close(REQUIRES)) {
		error("Can't close $file, $!");
		return 0;
	}

	return 1;
}

#
# Start of main program
#

my @installed;
my %requires;
my $pkg = "";
my $update_pkg = "";

use vars qw($opt_a $opt_c $opt_v $opt_r $opt_n);
getopts('acnvr:');

if ($opt_a && $opt_c) {
	die("Options 'a' and 'c' are mutually exclusive");
}

if ($opt_v) {
	$PKG_DELETE .= " -v";
	$PKG_ADD .= " -v";
	$PKG_CREATE .= " -v";
}

if ($opt_n) {
	$PKG_DELETE .= " -n";
	$PKG_ADD .= " -n";
}

if (scalar @ARGV < 1) {
	die("No package specified.\n");
} elsif (scalar @ARGV > 1) {
	die("Only one package may be updated at a time.\n");
}

my $pkgfile = $ARGV[0];
if (! -f $pkgfile) {
	die("Can't find package file $pkgfile\n");
}

my $newpkg = basename($pkgfile, '.tgz');
my ($pkgname, $new_version) = get_version($newpkg);

if ($opt_r && $opt_r ne "") {
	my ($old_pkg, $old_version) = get_version($opt_r);
	print "Updating $old_pkg package version ";
	print "$old_version to $new_version\n";
	$update_pkg = $opt_r;
} else {
	print "Updating $pkgname packages to version $new_version\n";
	$update_pkg = $pkgname;
}

# Safety net to prevent all packages getting deleted
if ($update_pkg eq "") {
	die ("Package to update is empty, aborting\n");
}

# Find out what package versions are already installed

open(PKGINFO, "$PKG_INFO|") || die("Can't run $PKG_INFO, $!");

while (<PKGINFO>) {
	my ($pkg) = /^(.*?)\s+.*/;

	if ($pkg =~ /^$update_pkg-[0-9\.]+/) {
		push(@installed, $pkg);
	}
}

close(PKGINFO) || die("Couldn't close pipe from $PKG_INFO, $!");

if (scalar @installed == 0) {
	if (! $opt_r) {
		die("There are no $pkgname packages installed.\n");
	} else {
		die("Package $opt_r is not installed.\n");
	}
}

# For each installed package that matches get the dependencies
my $old_pkg;
for $old_pkg (@installed) {
	if (! get_requires($old_pkg, \%requires)) {
		die("Failed to get requires from $old_pkg\n");
	}
}

# Now delete all currently installed packages
for $old_pkg (@installed) {
	if (! system("$PKG_DELETE $old_pkg")) {
		print "Deleted $old_pkg\n" if ($opt_v);
	} else {
		error("Couldn't remove package $old_pkg, $!");
	}
}

if (system("$PKG_ADD $pkgfile")) {
	error("Command '$PKG_ADD $newpkg' failed, $!");
	if (scalar keys %requires) {
		print "The following packages depended on previously\n";
		print "installed versions of $pkgname.\n";
		print "You need to add them to the +REQUIRES file when you\n";
		print "succeed in installing $newpkg.\n";
		my $req;
		for $req (keys %requires) {
			print $req, "\n";
		}
	}
} else {
	put_requires($pkgname . "-" . $new_version, \%requires);
}

exit;
