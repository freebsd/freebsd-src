#############################################################################  
# Pod/Find.pm -- finds files containing POD documentation
#
# Author: Marek Rouchal <marek@saftsack.fs.uni-bayreuth.de>
# 
# Copyright (C) 1999-2000 by Marek Rouchal (and borrowing code
# from Nick Ing-Simmon's PodToHtml). All rights reserved.
# This file is part of "PodParser". Pod::Find is free software;
# you can redistribute it and/or modify it under the same terms
# as Perl itself.
#############################################################################

package Pod::Find;

use vars qw($VERSION);
$VERSION = 0.12;   ## Current version of this package
require  5.005;    ## requires this Perl version or later

#############################################################################

=head1 NAME

Pod::Find - find POD documents in directory trees

=head1 SYNOPSIS

  use Pod::Find qw(pod_find simplify_name);
  my %pods = pod_find({ -verbose => 1, -inc => 1 });
  foreach(keys %pods) {
     print "found library POD `$pods{$_}' in $_\n";
  }

  print "podname=",simplify_name('a/b/c/mymodule.pod'),"\n";

=head1 DESCRIPTION

B<Pod::Find> provides a function B<pod_find> that searches for POD
documents in a given set of files and directories. It returns a hash
with the file names as keys and the POD name as value. The POD name
is derived from the file name and its position in the directory tree.

E.g. when searching in F<$HOME/perl5lib>, the file
F<$HOME/perl5lib/MyModule.pm> would get the POD name I<MyModule>,
whereas F<$HOME/perl5lib/Myclass/Subclass.pm> would be
I<Myclass::Subclass>. The name information can be used for POD
translators.

Only text files containing at least one valid POD command are found.

A warning is printed if more than one POD file with the same POD name
is found, e.g. F<CPAN.pm> in different directories. This usually
indicates duplicate occurrences of modules in the I<@INC> search path.

The function B<simplify_name> is equivalent to B<basename>, but also
strips Perl-like extensions (.pm, .pl, .pod) and extensions like
F<.bat>, F<.cmd> on Win32 and OS/2, respectively.

Note that neither B<pod_find> nor B<simplify_name> are exported by
default so be sure to specify them in the B<use> statement if you need
them:

  use Pod::Find qw(pod_find simplify_name);

=head1 OPTIONS

The first argument for B<pod_find> may be a hash reference with options.
The rest are either directories that are searched recursively or files.
The POD names of files are the plain basenames with any Perl-like extension
(.pm, .pl, .pod) stripped.

=over 4

=item B<-verbose>

Print progress information while scanning.

=item B<-perl>

Apply Perl-specific heuristics to find the correct PODs. This includes
stripping Perl-like extensions, omitting subdirectories that are numeric
but do I<not> match the current Perl interpreter's version id, suppressing
F<site_perl> as a module hierarchy name etc.

=item B<-script>

Search for PODs in the current Perl interpreter's installation 
B<scriptdir>. This is taken from the local L<Config|Config> module.

=item B<-inc>

Search for PODs in the current Perl interpreter's I<@INC> paths. This
automatically considers paths specified in the C<PERL5LIB> environment.

=back

=head1 AUTHOR

Marek Rouchal E<lt>marek@saftsack.fs.uni-bayreuth.deE<gt>,
heavily borrowing code from Nick Ing-Simmons' PodToHtml.

=head1 SEE ALSO

L<Pod::Parser>, L<Pod::Checker>

=cut

use strict;
#use diagnostics;
use Exporter;
use File::Spec;
use File::Find;
use Cwd;

use vars qw(@ISA @EXPORT_OK $VERSION);
@ISA = qw(Exporter);
@EXPORT_OK = qw(&pod_find &simplify_name);

# package global variables
my $SIMPLIFY_RX;

# return a hash of the POD files found
# first argument may be a hashref (options),
# rest is a list of directories to search recursively
sub pod_find
{
    my %opts;
    if(ref $_[0]) {
        %opts = %{shift()};
    }

    $opts{-verbose} ||= 0;
    $opts{-perl}    ||= 0;

    my (@search) = @_;

    if($opts{-script}) {
        require Config;
        push(@search, $Config::Config{scriptdir});
        $opts{-perl} = 1;
    }

    if($opts{-inc}) {
        push(@search, grep($_ ne '.',@INC));
        $opts{-perl} = 1;
    }

    if($opts{-perl}) {
        require Config;
        # this code simplifies the POD name for Perl modules:
        # * remove "site_perl"
        # * remove e.g. "i586-linux" (from 'archname')
        # * remove e.g. 5.00503
        # * remove pod/ if followed by *.pod (e.g. in pod/perlfunc.pod)
        $SIMPLIFY_RX =
          qq!^(?i:site_perl/|\Q$Config::Config{archname}\E/|\\d+\\.\\d+([_.]?\\d+)?/|pod/(?=.*?\\.pod\\z))*!;

    }

    my %dirs_visited;
    my %pods;
    my %names;
    my $pwd = cwd();

    foreach my $try (@search) {
        unless(File::Spec->file_name_is_absolute($try)) {
            # make path absolute
            $try = File::Spec->catfile($pwd,$try);
        }
        # simplify path
        $try = File::Spec->canonpath($try);
        my $name;
        if(-f $try) {
            if($name = _check_and_extract_name($try, $opts{-verbose})) {
                _check_for_duplicates($try, $name, \%names, \%pods);
            }
            next;
        }
        my $root_rx = qq!^\Q$try\E/!;
        File::Find::find( sub {
            my $item = $File::Find::name;
            if(-d) {
                if($dirs_visited{$item}) {
                    warn "Directory '$item' already seen, skipping.\n"
                        if($opts{-verbose});
                    $File::Find::prune = 1;
                    return;
                }
                else {
                    $dirs_visited{$item} = 1;
                }
                if($opts{-perl} && /^(\d+\.[\d_]+)\z/s && eval "$1" != $]) {
                    $File::Find::prune = 1;
                    warn "Perl $] version mismatch on $_, skipping.\n"
                        if($opts{-verbose});
                }
                return;
            }
            if($name = _check_and_extract_name($item, $opts{-verbose}, $root_rx)) {
                _check_for_duplicates($item, $name, \%names, \%pods);
            }
        }, $try); # end of File::Find::find
    }
    chdir $pwd;
    %pods;
}

sub _check_for_duplicates {
    my ($file, $name, $names_ref, $pods_ref) = @_;
    if($$names_ref{$name}) {
        warn "Duplicate POD found (shadowing?): $name ($file)\n";
        warn "    Already seen in ",
            join(' ', grep($$pods_ref{$_} eq $name, keys %$pods_ref)),"\n";
    }
    else {
        $$names_ref{$name} = 1;
    }
    $$pods_ref{$file} = $name;
}

sub _check_and_extract_name {
    my ($file, $verbose, $root_rx) = @_;

    # check extension or executable flag
    # this involves testing the .bat extension on Win32!
    unless($file =~ /\.(pod|pm|plx?)\z/i || (-f $file && -x _ && -T _)) {
        return undef;
    }

    # check for one line of POD
    unless(open(POD,"<$file")) {
        warn "Error: $file is unreadable: $!\n";
        return undef;
    }
    local $/ = undef;
    my $pod = <POD>;
    close(POD);
    unless($pod =~ /\n=(head\d|pod|over|item)\b/) {
        warn "No POD in $file, skipping.\n"
            if($verbose);
        return;
    }
    undef $pod;

    # strip non-significant path components
    # _TODO_ what happens on e.g. Win32?
    my $name = $file;
    if(defined $root_rx) {
        $name =~ s!$root_rx!!s;
        $name =~ s!$SIMPLIFY_RX!!os if(defined $SIMPLIFY_RX);
    }
    else {
        $name =~ s:^.*/::s;
    }
    _simplify($name);
    $name =~ s!/+!::!g; #/
    $name;
}

# basic simplification of the POD name:
# basename & strip extension
sub simplify_name {
    my ($str) = @_;
    # remove all path components
    $str =~ s:^.*/::s;
    _simplify($str);
    $str;
}

# internal sub only
sub _simplify {
    # strip Perl's own extensions
    $_[0] =~ s/\.(pod|pm|plx?)\z//i;
    # strip meaningless extensions on Win32 and OS/2
    $_[0] =~ s/\.(bat|exe|cmd)\z//i if($^O =~ /win|os2/i);
}

1;

