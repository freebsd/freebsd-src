#!/usr/local/bin/perl

use Config;
use File::Basename qw(&basename &dirname);
use Cwd;

# List explicitly here the variables you want Configure to
# generate.  Metaconfig only looks for shell variables, so you
# have to mention them as if they were shell variables, not
# %Config entries.  Thus you write
#  $startperl
# to ensure Configure will look for $Config{startperl}.

# This forces PL files to create target in same directory as PL file.
# This is so that make depend always knows where to find PL derivatives.
$origdir = cwd;
chdir(dirname($0));
($file = basename($0)) =~ s/\.PL$//;
$file =~ s/\.pl$// if ($^O eq 'os2' or $^O eq 'dos');  # "case-forgiving"
$file =~ s/\.pl$/.com/ if ($^O eq 'VMS');              # "case-forgiving"

open OUT,">$file" or die "Can't create $file: $!";

print "Extracting $file (with variable substitutions)\n";

# In this section, perl variables will be expanded during extraction.
# You can use $Config{...} to use Configure variables.

print OUT <<"!GROK!THIS!";
$Config{'startperl'}
    eval 'exec perl -S \$0 "\$@"'
        if 0;
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';

#############################################################################
# podselect -- command to invoke the podselect function in Pod::Select
#
# Copyright (c) 1996-2000 by Bradford Appleton. All rights reserved.
# This file is part of "PodParser". PodParser is free software;
# you can redistribute it and/or modify it under the same terms
# as Perl itself.
#############################################################################

use strict;
use diagnostics;

=head1 NAME

podselect - print selected sections of pod documentation on standard output

=head1 SYNOPSIS

B<podselect> [B<-help>] [B<-man>] [B<-section>S< >I<section-spec>]
[I<file>S< >...]

=head1 OPTIONS AND ARGUMENTS

=over 8

=item B<-help>

Print a brief help message and exit.

=item B<-man>

Print the manual page and exit.

=item B<-section>S< >I<section-spec>

Specify a section to include in the output.
See L<Pod::Parser/"SECTION SPECIFICATIONS">
for the format to use for I<section-spec>.
This option may be given multiple times on the command line.

=item I<file>

The pathname of a file from which to select sections of pod
documentation (defaults to standard input).

=back

=head1 DESCRIPTION

B<podselect> will read the given input files looking for pod
documentation and will print out (in raw pod format) all sections that
match one ore more of the given section specifications. If no section
specifications are given than all pod sections encountered are output.

B<podselect> invokes the B<podselect()> function exported by B<Pod::Select>
Please see L<Pod::Select/podselect()> for more details.

=head1 SEE ALSO

L<Pod::Parser> and L<Pod::Select>

=head1 AUTHOR

Brad Appleton E<lt>bradapp@enteract.comE<gt>

Based on code for B<Pod::Text::pod2text(1)> written by
Tom Christiansen E<lt>tchrist@mox.perl.comE<gt>

=cut

use Pod::Select;
use Pod::Usage;
use Getopt::Long;

## Define options
my %options = (
        "help"     => 0,
        "man"      => 0,
        "sections" => [],
);

## Parse options
GetOptions(\%options, "help", "man", "sections|select=s@")  ||  pod2usage(2);
pod2usage(1)  if ($options{help});
pod2usage(-verbose => 2)  if ($options{man});

## Dont default to STDIN if connected to a terminal
pod2usage(2) if ((@ARGV == 0) && (-t STDIN));

## Invoke podselect().
if (@{ $options{"sections"} } > 0) {
    podselect({ -sections => $options{"sections"} }, @ARGV);
}
else {
    podselect(@ARGV);
}


!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
