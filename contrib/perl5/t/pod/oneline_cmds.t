BEGIN {
   chdir 't' if -d 't';
   unshift @INC, '../lib';
   unshift @INC, './pod';
   require "testp2pt.pl";
   import TestPodIncPlainText;
}

my %options = map { $_ => 1 } @ARGV;  ## convert cmdline to options-hash
my $passed  = testpodplaintext \%options, $0;
exit( ($passed == 1) ? 0 : -1 )  unless $ENV{HARNESS_ACTIVE};


__END__


==head1 NAME
B<rdb2pg> - insert an rdb table into a PostgreSQL database

==head1 SYNOPSIS
B<rdb2pg>  [I<param>=I<value> ...]

==head1 PARAMETERS
B<rdb2pg> uses an IRAF-compatible parameter interface.  
A template parameter file is in F</proj/axaf/simul/lib/uparm/rdb2pg.par>.

==over 4
==item B<input> I<file>
The B<RDB> file to insert into the database. If the given name
is the string C<stdin>, it reads from the UNIX standard input stream.

==back

==head1 DESCRIPTION
B<rdb2pg> will enter the data from an B<RDB> database into a
PostgreSQL database table, optionally creating the database and the
table if they do not exist.  It automatically determines the
PostgreSQL data type from the column definition in the B<RDB> file,
but may be overriden via a series of definition files or directly
via one of its parameters.

The target database and table are specified by the C<db> and C<table>
parameters.  If they do not exist, and the C<createdb> parameter is
set, they will be created.  Table field definitions are determined
in the following order:

