#!/usr/bin/perl -w
# Copyright (c) 2002 Alexey Zelkin <phantom@FreeBSD.org>
#
# cknames.pl -- this scripts checks for existence persons listed
#               in authors.ent and access files
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
