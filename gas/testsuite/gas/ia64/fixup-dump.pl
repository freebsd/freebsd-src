print "# objdump: -d\n";
print "# name: ia64 $ARGV[0]\n";
shift;

while (<>) {
  if (/.*file format.*/) {
    $_ = ".*: +file format .*\n";
  } else {
    s/([][().])/\\$1/g;
  }
  print;
}
