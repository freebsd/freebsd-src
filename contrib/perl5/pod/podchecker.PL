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
$file =~ s/\.pl$//
        if ($^O eq 'VMS' or $^O eq 'os2' or $^O eq 'dos');  # "case-forgiving"
$file .= '.com' if $^O eq 'VMS';

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
# podchecker -- command to invoke the podchecker function in Pod::Checker
#
# Copyright (c) 1998-2000 by Bradford Appleton. All rights reserved.
# This file is part of "PodParser". PodParser is free software;
# you can redistribute it and/or modify it under the same terms
# as Perl itself.
#############################################################################

use strict;
#use diagnostics;

=head1 NAME

podchecker - check the syntax of POD format documentation files

=head1 SYNOPSIS

B<podchecker> [B<-help>] [B<-man>] [B<-(no)warnings>] [I<file>S< >...]

=head1 OPTIONS AND ARGUMENTS

=over 8

=item B<-help>

Print a brief help message and exit.

=item B<-man>

Print the manual page and exit.

=item B<-warnings> B<-nowarnings>

Turn on/off printing of warnings. Repeating B<-warnings> increases the
warning level, i.e. more warnings are printed. Currently increasing to
level two causes flagging of unescaped "E<lt>,E<gt>" characters.

=item I<file>

The pathname of a POD file to syntax-check (defaults to standard input).

=back

=head1 DESCRIPTION

B<podchecker> will read the given input files looking for POD
syntax errors in the POD documentation and will print any errors
it find to STDERR. At the end, it will print a status message
indicating the number of errors found.

Directories are ignored, an appropriate warning message is printed.

B<podchecker> invokes the B<podchecker()> function exported by B<Pod::Checker>
Please see L<Pod::Checker/podchecker()> for more details.

=head1 RETURN VALUE

B<podchecker> returns a 0 (zero) exit status if all specified
POD files are ok.

=head1 ERRORS

B<podchecker> returns the exit status 1 if at least one of
the given POD files has syntax errors.

The status 2 indicates that at least one of the specified 
files does not contain I<any> POD commands.

Status 1 overrides status 2. If you want unambigouus
results, call B<podchecker> with one single argument only.

=head1 SEE ALSO

L<Pod::Parser> and L<Pod::Checker>

=head1 AUTHORS

Brad Appleton E<lt>bradapp@enteract.comE<gt>,
Marek Rouchal E<lt>marek@saftsack.fs.uni-bayreuth.deE<gt>

Based on code for B<Pod::Text::pod2text(1)> written by
Tom Christiansen E<lt>tchrist@mox.perl.comE<gt>

=cut


use Pod::Checker;
use Pod::Usage;
use Getopt::Long;

## Define options
my %options;

## Parse options
GetOptions(\%options, qw(help man warnings+ nowarnings))  ||  pod2usage(2);
pod2usage(1)  if ($options{help});
pod2usage(-verbose => 2)  if ($options{man});

if($options{nowarnings}) {
  $options{warnings} = 0;
}
elsif(!defined $options{warnings}) {
  $options{warnings} = 1; # default is warnings on
}

## Dont default to STDIN if connected to a terminal
pod2usage(2) if ((@ARGV == 0) && (-t STDIN));

## Invoke podchecker()
my $status = 0;
@ARGV = qw(-) unless(@ARGV);
for (@ARGV) {
    if($_ eq '-') {
      $_ = "<&STDIN";
    }
    elsif(-d) {
      warn "podchecker: Warning: Ignoring directory '$_'\n";
      next;
    }
    my $s = podchecker($_, undef, '-warnings' => $options{warnings});
    if($s > 0) {
        # errors occurred
        $status = 1;
    }
    elsif($s < 0) {
        # no pod found
        $status = 2 unless($status);
    }
}
exit $status;

!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
