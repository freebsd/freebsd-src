# &open2: tom christiansen, <tchrist@convex.com>
#
# usage: $pid = &open2('rdr', 'wtr', 'some cmd and args');
#    or  $pid = &open2('rdr', 'wtr', 'some', 'cmd', 'and', 'args');
#
# spawn the given $cmd and connect $rdr for
# reading and $wtr for writing.  return pid
# of child, or 0 on failure.  
# 
# WARNING: this is dangerous, as you may block forever
# unless you are very careful.  
# 
# $wtr is left unbuffered.
# 
# abort program if
#	rdr or wtr are null
# 	pipe or fork or exec fails

package open2;
$fh = 'FHOPEN000';  # package static in case called more than once

sub main'open2 {
    local($kidpid);
    local($dad_rdr, $dad_wtr, @cmd) = @_;

    $dad_rdr ne '' 		|| die "open2: rdr should not be null";
    $dad_wtr ne '' 		|| die "open2: wtr should not be null";

    # force unqualified filehandles into callers' package
    local($package) = caller;
    $dad_rdr =~ s/^[^']+$/$package'$&/;
    $dad_wtr =~ s/^[^']+$/$package'$&/;

    local($kid_rdr) = ++$fh;
    local($kid_wtr) = ++$fh;

    pipe($dad_rdr, $kid_wtr) 	|| die "open2: pipe 1 failed: $!";
    pipe($kid_rdr, $dad_wtr) 	|| die "open2: pipe 2 failed: $!";

    if (($kidpid = fork) < 0) {
	die "open2: fork failed: $!";
    } elsif ($kidpid == 0) {
	close $dad_rdr; close $dad_wtr;
	open(STDIN,  "<&$kid_rdr");
	open(STDOUT, ">&$kid_wtr");
	warn "execing @cmd\n" if $debug;
	exec @cmd;
	die "open2: exec of @cmd failed";   
    } 
    close $kid_rdr; close $kid_wtr;
    select((select($dad_wtr), $| = 1)[0]); # unbuffer pipe
    $kidpid;
}
1; # so require is happy
