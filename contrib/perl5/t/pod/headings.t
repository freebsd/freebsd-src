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


#################################################################
  use Pod::Usage;
  pod2usage( VERBOSE => 2, EXIT => 1 );

=pod

=head1 NAME

B<rdb2pg> - insert an rdb table into a PostgreSQL database

=head1 SYNOPSIS

B<rdb2pg>  [I<param>=I<value> ...]

=head1 PARAMETERS

B<rdb2pg> uses an IRAF-compatible parameter interface.  
A template parameter file is in F</proj/axaf/simul/lib/uparm/rdb2pg.par>.

=over 4

=item B<input> I<file>

The B<RDB> file to insert into the database. If the given name
is the string C<stdin>, it reads from the UNIX standard input stream.


=back

=head1 DESCRIPTION

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

=cut

#################################################################

results in:


#################################################################

    rdb2pg - insert an rdb table into a PostgreSQL database

    rdb2pg [*param*=*value* ...]

    rdb2pg uses an IRAF-compatible parameter interface. A template
    parameter file is in /proj/axaf/simul/lib/uparm/rdb2pg.par.

    The RDB file to insert into the database. If the given name is
    the string `stdin', it reads from the UNIX standard input
    stream.

    rdb2pg will enter the data from an RDB database into a
    PostgreSQL database table, optionally creating the database and
    the table if they do not exist. It automatically determines the
    PostgreSQL data type from the column definition in the RDB file,
    but may be overriden via a series of definition files or
    directly via one of its parameters.

    The target database and table are specified by the `db' and
    `table' parameters. If they do not exist, and the `createdb'
    parameter is set, they will be created. Table field definitions
    are determined in the following order:


#################################################################

while the original version of Text (using pod2text) gives

#################################################################

NAME
    rdb2pg - insert an rdb table into a PostgreSQL database

SYNOPSIS
    rdb2pg [*param*=*value* ...]

PARAMETERS
    rdb2pg uses an IRAF-compatible parameter interface. A template
    parameter file is in /proj/axaf/simul/lib/uparm/rdb2pg.par.

    input *file*
        The RDB file to insert into the database. If the given name
        is the string `stdin', it reads from the UNIX standard input
        stream.

DESCRIPTION
    rdb2pg will enter the data from an RDB database into a
    PostgreSQL database table, optionally creating the database and
    the table if they do not exist. It automatically determines the
    PostgreSQL data type from the column definition in the RDB file,
    but may be overriden via a series of definition files or
    directly via one of its parameters.

    The target database and table are specified by the `db' and
    `table' parameters. If they do not exist, and the `createdb'
    parameter is set, they will be created. Table field definitions
    are determined in the following order:


#################################################################


Thanks for any help.  If, as your email indicates, you've not much
time to look at this, I can work around things by calling pod2text()
directly using the official Text.pm.

Diab

-------------
Diab Jerius
djerius@cfa.harvard.edu

