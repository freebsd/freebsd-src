# From: asherman@fmrco.com (Aaron Sherman)

sub hostname
{
	local(*P,@tmp,$hostname,$_);
	if (open(P,"hostname 2>&1 |") && (@tmp = <P>) && close(P))
	{
		chop($hostname = $tmp[$#tmp]);
	}
	elsif (open(P,"uname -n 2>&1 |") && (@tmp = <P>) && close(P))
	{
		chop($hostname = $tmp[$#tmp]);
	}
	else
	{
		die "$0: Cannot get hostname from 'hostname' or 'uname -n'\n";
	}
	@tmp = ();
	close P; # Just in case we failed in an odd spot....
	$hostname;
}

1;
