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
chdir dirname($0);
$file = basename($0, '.PL');
$file .= '.com' if $^O eq 'VMS';

open OUT,">$file" or die "Can't create $file: $!";

print "Extracting $file (with variable substitutions)\n";

# In this section, perl variables will be expanded during extraction.
# You can use $Config{...} to use Configure variables.

print OUT <<"!GROK!THIS!";
$Config{startperl}
    eval 'exec $Config{perlpath} -S \$0 \${1+"\$@"}'
	if \$running_under_some_shell;
!GROK!THIS!

# In the following, perl variables are not expanded during extraction.

print OUT <<'!NO!SUBS!';

# pod2latex conversion program

use Pod::LaTeX;
use Pod::Find qw/ pod_find /;
use Pod::Usage;
use Getopt::Long;
use File::Basename;

# Read command line arguments

my %options = (
	       "help"   => 0,
	       "man"    => 0,
	       "sections" => [],
	       "full"   => 0,
	       "out"    => undef,
	       "verbose" => 0,
	       "modify" => 0,
	      );

GetOptions(\%options, 
	   "help",
	   "man",
	   "verbose",
	   "full",
	   "sections=s@",
	   "out=s",
	   "modify",
	  ) || pod2usage(2);

pod2usage(1)  if ($options{help});
pod2usage(-verbose => 2)  if ($options{man});


# Read all the files from the command line
my @files = @ARGV;

# Now find which ones are real pods and convert 
# directories to their contents.

# Extract the pods from each arg since some of them might
# be directories
# This is not as efficient as using pod_find to search through
# everything at once but it allows us to preserve the order 
# supplied by the user

my @pods;
foreach my $arg (@files) {
  my %pods = pod_find($arg);
  push(@pods, sort keys %pods);
}

# Abort if nothing to do
if ($#pods == -1) {
  warn "None of the supplied Pod files actually exist\n";
  exit;
}



# If $options{'out'} is set we are processing to a single output file
my $multi_documents;
if (exists $options{'out'} && defined $options{'out'}) {
  $multi_documents = 0;
} else {
  $multi_documents = 1;
}

# If the output file is not specified it is assumed that
# a single output file is required per input file using
# a .tex extension rather than any exisiting extension

if ($multi_documents) {

  # Case where we just generate one input per output

  foreach my $pod (@pods) {

    if (-f $pod) {

      my $output = $pod;
      $output = basename($output, '.pm', '.pod','.pl') . '.tex';

      # Create a new parser object
      my $parser = new Pod::LaTeX(
				  AddPreamble => $options{'full'},
				  AddPostamble => $options{'full'},
				  MakeIndex => $options{'full'},
				  TableOfContents => $options{'full'},
				  ReplaceNAMEwithSection => $options{'modify'},
				  UniqueLabels => $options{'modify'},
				 );

      # Select sections if supplied
      $parser->select(@{ $options{'sections'}})
	if @{$options{'sections'}};

      # Derive the input file from the output file
      $parser->parse_from_file($pod, $output);

      print "Written output to $output\n" if $options{'verbose'};

    } else {
      warn "File $pod not found\n";
    }

  }
} else {
  
  # Case where we want everything to be in a single document

  # Need to open the output file ourselves
  my $output = $options{'out'};
  $output .= '.tex' unless $output =~ /\.tex$/;

  # Use auto-vivified file handle in perl 5.6
  use Symbol;
  my $outfh = gensym;
  open ($outfh, ">$output") || die "Could not open output file: $!\n";

  # Flag to indicate whether we have converted at least one file
  # indicates how many files have been converted
  my $converted = 0;

  # Loop over the input files
  foreach my $pod (@pods) {

    if (-f $pod) {

      warn "Converting $pod\n" if $options{'verbose'};

      # Open the file (need the handle)
      # Use auto-vivified handle in perl 5.6
      my $podfh = gensym;
      open ($podfh, "<$pod") || die "Could not open pod file $pod: $!\n";

      # if this is the first file to be converted we may want to add
      # a preamble (controlled by command line option)
      if ($converted == 0 && $options{'full'}) {
	$preamble = 1;
      } else {
	$preamble = 0;
      }

      # if this is the last file to be converted may want to add
      # a postamble (controlled by command line option)
      # relies on a previous pass to check existence of all pods we
      # are converting.
      my $postamble = ( ($converted == $#pods && $options{'full'}) ? 1 : 0 );

      # Open parser object
      # May want to start with a preamble for the first one and
      # end with an index for the last
      my $parser = new Pod::LaTeX(
				  MakeIndex => $options{'full'},
				  TableOfContents => $preamble,
				  ReplaceNAMEwithSection => $options{'modify'},
				  UniqueLabels => $options{'modify'},
				  StartWithNewPage => $options{'full'},
				  AddPreamble => $preamble,
				  AddPostamble => $postamble,
				 );

      # Store the file name for error messages
      # This is a kluge that breaks the data hiding of the object
      $parser->{_INFILE} = $pod;

      # Select sections if supplied
      $parser->select(@{ $options{'sections'}})
	if @{$options{'sections'}};

      # Parse it
      $parser->parse_from_filehandle($podfh, $outfh);

      # We have converted at least one file
      $converted++;

    } else {
      warn "File $pod not found\n";
    }

  }

  # Should unlink the file if we didn't convert anything!
  # dont check for return status of unlink
  # since there is not a lot to be done if the unlink failed
  # and the program does not rely upon it.
  unlink "$output" unless $converted;

  # If verbose
  warn "Converted $converted files\n" if $options{'verbose'};

}

exit;

__END__

=head1 NAME

pod2latex - convert pod documentation to latex format

=head1 SYNOPSIS

  pod2latex *.pm

  pod2latex -out mytex.tex *.pod

  pod2latex -full -sections 'DESCRIPTION|NAME' SomeDir

=head1 DESCRIPTION

C<pod2latex> is a program to convert POD format documentation
(L<perlpod>) into latex. It can process multiple input documents at a
time and either generate a latex file per input document or a single
combined output file.

=head1 OPTIONS AND ARGUMENTS

This section describes the supported command line options. Minium
matching is supported.

=over 4

=item B<-out>

Name of the output file to be used. If there are multiple input pods
it is assumed that the intention is to write all translated output
into a single file. C<.tex> is appended if not present.  If the
argument is not supplied, a single document will be created for each
input file.

=item B<-full>

Creates a complete C<latex> file that can be processed immediately
(unless C<=for/=begin> directives are used that rely on extra packages).
Table of contents and index generation commands are included in the
wrapper C<latex> code.

=item B<-sections>

Specify pod sections to include (or remove if negated) in the
translation.  See L<Pod::Select/"SECTION SPECIFICATIONS"> for the
format to use for I<section-spec>. This option may be given multiple
times on the command line.This is identical to the similar option in
the C<podselect()> command.

=item B<-modify>

This option causes the output C<latex> to be slightly
modified from the input pod such that when a C<=head1 NAME>
is encountered a section is created containing the actual
pod name (rather than B<NAME>) and all subsequent C<=head1>
directives are treated as subsections. This has the advantage
that the description of a module will be in its own section
which is helpful for including module descriptions in documentation.
Also forces C<latex> label and index entries to be prefixed by the
name of the module.

=item B<-help>

Print a brief help message and exit.

=item B<-man>

Print the manual page and exit.

=item B<-verbose>

Print information messages as each document is processed.

=back

=head1 BUGS

Known bugs are:

=over 4

=item * 

Cross references between documents are not resolved when multiple
pod documents are converted into a single output C<latex> file.

=item * 

Functions and variables are not automatically recognized 
and they will therefore not be marked up in any special way
unless instructed by an explicit pod command.

=back

=head1 SEE ALSO

L<Pod::LaTeX>

=head1 AUTHOR

Tim Jenness E<lt>t.jenness@jach.hawaii.eduE<gt>

This program is free software; you can redistribute it
and/or modify it under the same terms as Perl itself.

Copyright (C) 2000 Tim Jenness.

=cut

!NO!SUBS!

close OUT or die "Can't close $file: $!";
chmod 0755, $file or die "Can't reset permissions for $file: $!\n";
exec("$Config{'eunicefix'} $file") if $Config{'eunicefix'} ne ':';
chdir $origdir;
