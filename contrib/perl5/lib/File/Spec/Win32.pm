package File::Spec::Win32;

=head1 NAME

File::Spec::Win32 - methods for Win32 file specs

=head1 SYNOPSIS

 use File::Spec::Win32; # Done internally by File::Spec if needed

=head1 DESCRIPTION

See File::Spec::Unix for a documentation of the methods provided
there. This package overrides the implementation of these methods, not
the semantics.

=over

=cut 

#use Config;
#use Cwd;
use File::Basename;
require Exporter;
use strict;

use vars qw(@ISA);

use File::Spec;
Exporter::import('File::Spec', qw( $Verbose));

@ISA = qw(File::Spec::Unix);

$ENV{EMXSHELL} = 'sh'; # to run `commands`

sub file_name_is_absolute {
    my($self,$file) = @_;
    $file =~ m{^([a-z]:)?[\\/]}i ;
}

sub catdir {
    my $self = shift;
    my @args = @_;
    for (@args) {
	# append a slash to each argument unless it has one there
	$_ .= "\\" if $_ eq '' or substr($_,-1) ne "\\";
    }
    my $result = $self->canonpath(join('', @args));
    $result;
}

=item catfile

Concatenate one or more directory names and a filename to form a
complete path ending with a filename

=cut

sub catfile {
    my $self = shift @_;
    my $file = pop @_;
    return $file unless @_;
    my $dir = $self->catdir(@_);
    $dir =~ s/(\\\.)$//;
    $dir .= "\\" unless substr($dir,length($dir)-1,1) eq "\\";
    return $dir.$file;
}

sub path {
    local $^W = 1;
    my($self) = @_;
    my $path = $ENV{'PATH'} || $ENV{'Path'} || $ENV{'path'};
    my @path = split(';',$path);
    foreach(@path) { $_ = '.' if $_ eq '' }
    @path;
}

=item canonpath

No physical check on the filesystem, but a logical cleanup of a
path. On UNIX eliminated successive slashes and successive "/.".

=cut

sub canonpath {
    my($self,$path) = @_;
    $path =~ s/^([a-z]:)/\u$1/;
    $path =~ s|/|\\|g;
    $path =~ s|\\+|\\|g ;                          # xx////xx  -> xx/xx
    $path =~ s|(\\\.)+\\|\\|g ;                    # xx/././xx -> xx/xx
    $path =~ s|^(\.\\)+|| unless $path eq ".\\";   # ./xx      -> xx
    $path =~ s|\\$|| 
             unless $path =~ m#^([a-z]:)?\\#;      # xx/       -> xx
    $path .= '.' if $path =~ m#\\$#;
    $path;
}

1;
__END__

=back

=cut 

