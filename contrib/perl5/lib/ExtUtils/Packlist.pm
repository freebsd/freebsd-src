package ExtUtils::Packlist;

use 5.005_64;
use strict;
use Carp qw();
our $VERSION = '0.03';

# Used for generating filehandle globs.  IO::File might not be available!
my $fhname = "FH1";

sub mkfh()
{
no strict;
my $fh = \*{$fhname++};
use strict;
return($fh);
}

sub new($$)
{
my ($class, $packfile) = @_;
$class = ref($class) || $class;
my %self;
tie(%self, $class, $packfile);
return(bless(\%self, $class));
}

sub TIEHASH
{
my ($class, $packfile) = @_;
my $self = { packfile => $packfile };
bless($self, $class);
$self->read($packfile) if (defined($packfile) && -f $packfile);
return($self);
}

sub STORE
{
$_[0]->{data}->{$_[1]} = $_[2];
}

sub FETCH
{
return($_[0]->{data}->{$_[1]});
}

sub FIRSTKEY
{
my $reset = scalar(keys(%{$_[0]->{data}}));
return(each(%{$_[0]->{data}}));
}

sub NEXTKEY
{
return(each(%{$_[0]->{data}}));
}

sub EXISTS
{
return(exists($_[0]->{data}->{$_[1]}));
}

sub DELETE
{
return(delete($_[0]->{data}->{$_[1]}));
}

sub CLEAR
{
%{$_[0]->{data}} = ();
}

sub DESTROY
{
}

sub read($;$)
{
my ($self, $packfile) = @_;
$self = tied(%$self) || $self;

if (defined($packfile)) { $self->{packfile} = $packfile; }
else { $packfile = $self->{packfile}; }
Carp::croak("No packlist filename specified") if (! defined($packfile));
my $fh = mkfh();
open($fh, "<$packfile") || Carp::croak("Can't open file $packfile: $!");
$self->{data} = {};
my ($line);
while (defined($line = <$fh>))
   {
   chomp $line;
   my ($key, @kvs) = split(' ', $line);
   $key =~ s!/\./!/!g;   # Some .packlists have spurious '/./' bits in the paths
   if (! @kvs)
      {
      $self->{data}->{$key} = undef;
      }
   else
      {
      my ($data) = {};
      foreach my $kv (@kvs)
         {
         my ($k, $v) = split('=', $kv);
         $data->{$k} = $v;
         }
      $self->{data}->{$key} = $data;
      }
   }
close($fh);
}

sub write($;$)
{
my ($self, $packfile) = @_;
$self = tied(%$self) || $self;
if (defined($packfile)) { $self->{packfile} = $packfile; }
else { $packfile = $self->{packfile}; }
Carp::croak("No packlist filename specified") if (! defined($packfile));
my $fh = mkfh();
open($fh, ">$packfile") || Carp::croak("Can't open file $packfile: $!");
foreach my $key (sort(keys(%{$self->{data}})))
   {
   print $fh ("$key");
   if (ref($self->{data}->{$key}))
      {
      my $data = $self->{data}->{$key};
      foreach my $k (sort(keys(%$data)))
         {
         print $fh (" $k=$data->{$k}");
         }
      }
   print $fh ("\n");
   }
close($fh);
}

sub validate($;$)
{
my ($self, $remove) = @_;
$self = tied(%$self) || $self;
my @missing;
foreach my $key (sort(keys(%{$self->{data}})))
   {
   if (! -e $key)
      {
      push(@missing, $key);
      delete($self->{data}{$key}) if ($remove);
      }
   }
return(@missing);
}

sub packlist_file($)
{
my ($self) = @_;
$self = tied(%$self) || $self;
return($self->{packfile});
}

1;

__END__

=head1 NAME

ExtUtils::Packlist - manage .packlist files

=head1 SYNOPSIS

   use ExtUtils::Packlist;
   my ($pl) = ExtUtils::Packlist->new('.packlist');
   $pl->read('/an/old/.packlist');
   my @missing_files = $pl->validate();
   $pl->write('/a/new/.packlist');

   $pl->{'/some/file/name'}++;
      or
   $pl->{'/some/other/file/name'} = { type => 'file',
                                      from => '/some/file' };

=head1 DESCRIPTION

ExtUtils::Packlist provides a standard way to manage .packlist files.
Functions are provided to read and write .packlist files.  The original
.packlist format is a simple list of absolute pathnames, one per line.  In
addition, this package supports an extended format, where as well as a filename
each line may contain a list of attributes in the form of a space separated
list of key=value pairs.  This is used by the installperl script to
differentiate between files and links, for example.

=head1 USAGE

The hash reference returned by the new() function can be used to examine and
modify the contents of the .packlist.  Items may be added/deleted from the
.packlist by modifying the hash.  If the value associated with a hash key is a
scalar, the entry written to the .packlist by any subsequent write() will be a
simple filename.  If the value is a hash, the entry written will be the
filename followed by the key=value pairs from the hash.  Reading back the
.packlist will recreate the original entries.

=head1 FUNCTIONS

=over

=item new()

This takes an optional parameter, the name of a .packlist.  If the file exists,
it will be opened and the contents of the file will be read.  The new() method
returns a reference to a hash.  This hash holds an entry for each line in the
.packlist.  In the case of old-style .packlists, the value associated with each
key is undef.  In the case of new-style .packlists, the value associated with
each key is a hash containing the key=value pairs following the filename in the
.packlist.

=item read()

This takes an optional parameter, the name of the .packlist to be read.  If
no file is specified, the .packlist specified to new() will be read.  If the
.packlist does not exist, Carp::croak will be called.

=item write()

This takes an optional parameter, the name of the .packlist to be written.  If
no file is specified, the .packlist specified to new() will be overwritten.

=item validate()

This checks that every file listed in the .packlist actually exists.  If an
argument which evaluates to true is given, any missing files will be removed
from the internal hash.  The return value is a list of the missing files, which
will be empty if they all exist.

=item packlist_file()

This returns the name of the associated .packlist file

=back

=head1 EXAMPLE

Here's C<modrm>, a little utility to cleanly remove an installed module.

    #!/usr/local/bin/perl -w

    use strict;
    use IO::Dir;
    use ExtUtils::Packlist;
    use ExtUtils::Installed;

    sub emptydir($) {
	my ($dir) = @_;
	my $dh = IO::Dir->new($dir) || return(0);
	my @count = $dh->read();
	$dh->close();
	return(@count == 2 ? 1 : 0);
    }

    # Find all the installed packages
    print("Finding all installed modules...\n");
    my $installed = ExtUtils::Installed->new();

    foreach my $module (grep(!/^Perl$/, $installed->modules())) {
       my $version = $installed->version($module) || "???";
       print("Found module $module Version $version\n");
       print("Do you want to delete $module? [n] ");
       my $r = <STDIN>; chomp($r);
       if ($r && $r =~ /^y/i) {
	  # Remove all the files
	  foreach my $file (sort($installed->files($module))) {
	     print("rm $file\n");
	     unlink($file);
	  }
	  my $pf = $installed->packlist($module)->packlist_file();
	  print("rm $pf\n");
	  unlink($pf);
	  foreach my $dir (sort($installed->directory_tree($module))) {
	     if (emptydir($dir)) {
		print("rmdir $dir\n");
		rmdir($dir);
	     }
	  }
       }
    }

=head1 AUTHOR

Alan Burlison <Alan.Burlison@uk.sun.com>

=cut
