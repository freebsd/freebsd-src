#!/usr/bin/perl
# this program creates a database in ARGV[1] from pairs given on 
# stdandard input
#
# $Id: create.pl,v 1.1.1.1 2000/06/20 22:12:09 agmorgan Exp $

use DB_File;

my $database = $ARGV[0];
die "Use: check,pl <database>\n" unless ($database);
print "Using database: $database\n";

my %lusers = ();

tie %lusers, 'DB_File', $database, O_RDWR|O_CREAT, 0644, $DB_HASH ;
while (<STDIN>) {
  my ($user, $pass) = split;

  $lusers{$user} = $pass;
}
untie %lusers;


