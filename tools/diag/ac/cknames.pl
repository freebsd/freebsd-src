#!/usr/bin/perl -w
#
# Copyright (c) 2002 Alexey Zelkin <phantom@FreeBSD.org>
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
#
# cknames.pl -- this scripts checks for integrity of person lists
#               between authors.ent, CVSROOT/access and passwd database
#               at freefall.freebsd.org
#
# NOTE: This script is supposed to run at freefall.freebsd.org *only*
#
# $FreeBSD$
#

$debug = 0;
$accessfile = "CVSROOT/access";
$authorsfile = "doc/en_US.ISO8859-1/share/sgml/authors.ent";

$cvsroot = $ENV{'CVSROOT'};
$cvsroot = "/home/ncvs" if !$cvsroot;
$cvs = "cvs -R -d $cvsroot co -p";

open(PASSWD, 'ypcat passwd |') || die "open passwd data: $!\n";
while (<PASSWD>) {
	($login,undef) = split(/:/);
	$login =~ s/_//g;	# remove _ from usernames since this
				# character is not allowed in docbook entities
	print "passwd user: $login\n" if $debug;
	$users{$login} = 1;
}
close PASSWD;

print "$cvs $accessfile\n";
open (ACCESS, "$cvs $accessfile |") || die "checkout $accessfile: $!\n";
while (<ACCESS>) {
	chomp;
	next if /^#/;
	($accuser, undef) = split /\s/;
	$accuser =~ s/_//g;
	print "access user: $accuser\n" if $debug;
	$access{$accuser} = 1;
}
close ACCESS;

open (AUTHORS, "$cvs $authorsfile |") || die "checkout $authorsfile: $!\n";
while (<AUTHORS>) {
	$author = $1 if /ENTITY a\.([^ ]+)/;
	next if !$author;
	print "authors entity: $author\n" if $debug;
	$authors{$author} = 1;
	$author = "";
}
close AUTHORS;

print "\n";
print "People listed in CVSROOT/access, but have no account\n";
print "----------------------------------------------------\n";
foreach (keys %access) {
	print "$_\n" if (!defined $users{$_});
}

print "\n";
print "People listed in autors.ent, not have no account\n";
print "------------------------------------------------\n";
foreach (keys %authors) {
	print "$_\n" if (!defined $users{$_});
}

print "\n";
print "People listed in CVSROOT/access, but not listed in authors.ent\n";
print "--------------------------------------------------------------\n";
foreach (keys %access) {
	print "$_\n" if (!defined $authors{$_});
}

print "\n";
