#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

print "1..6\n";

@oops = @ops = <op/*>;

if ($^O eq 'MSWin32') {
  map { $files{lc($_)}++ } <op/*>;
  map { delete $files{"op/$_"} } split /[\s\n]/, `cmd /c "dir /b /l op & dir /b /l /ah op 2>nul"`,
}
else {
  map { $files{$_}++ } <op/*>;
  map { delete $files{$_} } split /[\s\n]/, `echo op/*`;
}
if (keys %files) {
	print "not ok 1\t(",join(' ', sort keys %files),"\n";
} else { print "ok 1\n"; }

print $/ eq "\n" ? "ok 2\n" : "not ok 2\n";

while (<jskdfjskdfj* op/* jskdjfjkosvk*>) {
    $not = "not " unless $_ eq shift @ops;
    $not = "not at all " if $/ eq "\0";
}
print "${not}ok 3\n";

print $/ eq "\n" ? "ok 4\n" : "not ok 4\n";

# test the "glob" operator
$_ = "op/*";
@glops = glob $_;
print "@glops" eq "@oops" ? "ok 5\n" : "not ok 5\n";

@glops = glob;
print "@glops" eq "@oops" ? "ok 6\n" : "not ok 6\n";
