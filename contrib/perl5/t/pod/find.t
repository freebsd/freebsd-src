# Testing of Pod::Find
# Author: Marek Rouchal <marek@saftsack.fs.uni-bayreuth.de>

BEGIN {
    chdir 't' if -d 't';
    unshift @INC, '../lib';
}

$| = 1;

use Test;

BEGIN { plan tests => 4 }

use Pod::Find qw(pod_find pod_where);
use File::Spec;

# load successful
ok(1);

require Cwd;
my $THISDIR = Cwd::cwd();
my $VERBOSE = 0;
my $lib_dir = File::Spec->catdir($THISDIR,'..','lib','Pod');
if ($^O eq 'VMS') {
    $lib_dir = VMS::Filespec::unixify(File::Spec->catdir($THISDIR,'-','lib','pod'));
    $Qlib_dir = $lib_dir;
    $Qlib_dir =~ s#\/#::#g;
}
print "### searching $lib_dir\n";
my %pods = pod_find("$lib_dir");
my $result = join(",", sort values %pods);
print "### found $result\n";
my $compare = join(',', qw(
    Checker
    Find
    Html
    InputObjects
    LaTeX
    Man
    ParseUtils
    Parser
    Plainer
    Select
    Text
    Text::Color
    Text::Overstrike
    Text::Termcap
    Usage
));
if ($^O eq 'VMS') {
    $compare = lc($compare);
    $result = join(',', sort grep(/pod::/, values %pods));
    my $undollared = $Qlib_dir;
    $undollared =~ s/\$/\\\$/g;
    $undollared =~ s/\-/\\\-/g;
    $result =~ s/$undollared/pod::/g;
    my $count = 0;
    my @result = split(/,/,$result);
    my @compare = split(/,/,$compare);
    foreach(@compare) {
        $count += grep {/$_/} @result;
    }
    ok($count/($#result+1)-1,$#compare);
}
else {
    ok($result,$compare);
}

# File::Find is located in this place since eons
# and on all platforms, hopefully

print "### searching for File::Find\n";
$result = pod_where({ -inc => 1, -verbose => $VERBOSE }, 'File::Find')
  || 'undef - pod not found!';
print "### found $result\n";

if ($^O eq 'VMS') { # privlib is perl_root:[lib] OK but not under mms
    $compare = "lib.File]Find.pm";
    $result =~ s/perl_root:\[\-?\.?//i;
    $result =~ s/\[\-?\.?//i; # needed under `mms test`
    ok($result,$compare);
}
else {
    $compare = File::Spec->catfile("..","lib","File","Find.pm");
    ok(_canon($result),_canon($compare));
}

# Search for a documentation pod rather than a module
print "### searching for perlfunc.pod\n";
$result = pod_where({ -dirs => ['../pod'], -verbose => $VERBOSE }, 'perlfunc')
  || 'undef - perlfunc.pod not found!';
print "### found $result\n";

if ($^O eq 'VMS') { # privlib is perl_root:[lib] unfortunately
    $compare = "/lib/pod/perlfunc.pod";
    $result = VMS::Filespec::unixify($result);
    $result =~ s/perl_root\///i;
    $result =~ s/^\.\.//;  # needed under `mms test`
    ok($result,$compare);
}
else {
    $compare = File::Spec->catfile("..","pod","perlfunc.pod");
    ok(_canon($result),_canon($compare));
}

# make the path as generic as possible
sub _canon
{
  my ($path) = @_;
  $path = File::Spec->canonpath($path);
  my @comp = File::Spec->splitpath($path);
  my @dir = File::Spec->splitdir($comp[1]);
  $comp[1] = File::Spec->catdir(@dir);
  $path = File::Spec->catpath(@dir);
  $path = uc($path) if File::Spec->case_tolerant;
  $path;
}

