#!./perl

$skip_amp = 1;
for $file ('op/regexp.t', 't/op/regexp.t') {
  if (-r $file) {
    do $file;
    exit;
  }
}
die "Cannot find op/regexp.t or t/op/regexp.t\n";
