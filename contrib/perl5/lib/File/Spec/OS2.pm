package File::Spec::OS2;

#use Config;
#use Cwd;
#use File::Basename;
use strict;
require Exporter;

use File::Spec;
use vars qw(@ISA);

Exporter::import('File::Spec',
       qw( $Verbose));

@ISA = qw(File::Spec::Unix);

$ENV{EMXSHELL} = 'sh'; # to run `commands`

sub file_name_is_absolute {
    my($self,$file) = @_;
    $file =~ m{^([a-z]:)?[\\/]}i ;
}

sub path {
    my($self) = @_;
    my $path_sep = ";";
    my $path = $ENV{PATH};
    $path =~ s:\\:/:g;
    my @path = split $path_sep, $path;
    foreach(@path) { $_ = '.' if $_ eq '' }
    @path;
}

1;
__END__

=head1 NAME

File::Spec::OS2 - methods for OS/2 file specs

=head1 SYNOPSIS

 use File::Spec::OS2; # Done internally by File::Spec if needed

=head1 DESCRIPTION

See File::Spec::Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=cut
