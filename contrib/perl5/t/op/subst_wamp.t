#!./perl

$dummy = defined $&;		# Now we have it...
for $file ('op/subst.t', 't/op/subst.t') {
  if (-r $file) {
    do $file;
    exit;
  }
}
die "Cannot find op/subst.t or t/op/subst.t\n";

