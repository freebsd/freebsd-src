#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
    eval {my @n = getpwuid 0};
    if ($@ && $@ =~ /(The \w+ function is unimplemented)/) {
	print "1..0 # Skip: $1\n";
	exit 0;
    }
    eval { require Config; import Config; };
    my $reason;
    if ($Config{'i_pwd'} ne 'define') {
	$reason = '$Config{i_pwd} undefined';
    }
    elsif (not -f "/etc/passwd" ) { # Play safe.
	$reason = 'no /etc/passwd file';
    }

    if (not defined $where) {	# Try NIS.
	foreach my $ypcat (qw(/usr/bin/ypcat /bin/ypcat /etc/ypcat)) {
	    if (-x $ypcat &&
		open(PW, "$ypcat passwd 2>/dev/null |") &&
		defined(<PW>)) {
		$where = "NIS passwd";
		undef $reason;
		last;
	    }
	}
    }

    if (not defined $where) {	# Try NetInfo.
	foreach my $nidump (qw(/usr/bin/nidump)) {
	    if (-x $nidump &&
		open(PW, "$nidump passwd . 2>/dev/null |") &&
		defined(<PW>)) {
		$where = "NetInfo passwd";
		undef $reason;
		last;
	    }
	}
    }

    if (not defined $where) {	# Try local.
	my $PW = "/etc/passwd";
	if (-f $PW && open(PW, $PW) && defined(<PW>)) {
	    $where = $PW;
	    undef $reason;
	}
    }

    if ($reason) {	# Give up.
	print "1..0 # Skip: $reason\n";
	exit 0;
    }
}

# By now the PW filehandle should be open and full of juicy password entries.

print "1..2\n";

# Go through at most this many users.
# (note that the first entry has been read away by now)
my $max = 25;

my $n = 0;
my $tst = 1;
my %perfect;
my %seen;

setpwent();
while (<PW>) {
    chomp;
    # LIMIT -1 so that users with empty shells don't fall off
    my @s = split /:/, $_, -1;
    my ($name_s, $passwd_s, $uid_s, $gid_s, $gcos_s, $home_s, $shell_s);
    if ($^O eq 'darwin') {
       ($name_s, $passwd_s, $uid_s, $gid_s, $gcos_s, $home_s, $shell_s) = @s[0,1,2,3,7,8,9];
    } else {
       ($name_s, $passwd_s, $uid_s, $gid_s, $gcos_s, $home_s, $shell_s) = @s;
    }
    next if /^\+/; # ignore NIS includes
    if (@s) {
	push @{ $seen{$name_s} }, $.;
    } else {
	warn "# Your $where line $. is empty.\n";
	next;
    }
    if ($n == $max) {
	local $/;
	my $junk = <PW>;
	last;
    }
    # In principle we could whine if @s != 7 but do we know enough
    # of passwd file formats everywhere?
    if (@s == 7 || ($^O eq 'darwin' && @s == 10)) {
	@n = getpwuid($uid_s);
	# 'nobody' et al.
	next unless @n;
	my ($name,$passwd,$uid,$gid,$quota,$comment,$gcos,$home,$shell) = @n;
	# Protect against one-to-many and many-to-one mappings.
	if ($name_s ne $name) {
	    @n = getpwnam($name_s);
	    ($name,$passwd,$uid,$gid,$quota,$comment,$gcos,$home,$shell) = @n;
	    next if $name_s ne $name;
	}
	$perfect{$name_s}++
	    if $name    eq $name_s    and
               $uid     eq $uid_s     and
# Do not compare passwords: think shadow passwords.
               $gid     eq $gid_s     and
               $gcos    eq $gcos_s    and
               $home    eq $home_s    and
               $shell   eq $shell_s;
    }
    $n++;
}
endpwent();

if (keys %perfect == 0) {
    $max++;
    print <<EOEX;
#
# The failure of op/pwent test is not necessarily serious.
# It may fail due to local password administration conventions.
# If you are for example using both NIS and local passwords,
# test failure is possible.  Any distributed password scheme
# can cause such failures.
#
# What the pwent test is doing is that it compares the $max first
# entries of $where
# with the results of getpwuid() and getpwnam() call.  If it finds no
# matches at all, it suspects something is wrong.
# 
EOEX
    print "not ";
    $not = 1;
} else {
    $not = 0;
}
print "ok ", $tst++;
print "\t# (not necessarily serious: run t/op/pwent.t by itself)" if $not;
print "\n";

# Test both the scalar and list contexts.

my @pw1;

setpwent();
for (1..$max) {
    my $pw = scalar getpwent();
    last unless defined $pw;
    push @pw1, $pw;
}
endpwent();

my @pw2;

setpwent();
for (1..$max) {
    my ($pw) = (getpwent());
    last unless defined $pw;
    push @pw2, $pw;
}
endpwent();

print "not " unless "@pw1" eq "@pw2";
print "ok ", $tst++, "\n";

close(PW);
