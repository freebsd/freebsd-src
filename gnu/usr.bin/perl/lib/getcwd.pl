# By Brandon S. Allbery
#
# Usage: $cwd = &getcwd;

sub getcwd
{
    local($dotdots, $cwd, @pst, @cst, $dir, @tst);

    unless (@cst = stat('.'))
    {
	warn "stat(.): $!";
	return '';
    }
    $cwd = '';
    do
    {
	$dotdots .= '/' if $dotdots;
	$dotdots .= '..';
	@pst = @cst;
	unless (opendir(getcwd'PARENT, $dotdots))			#'))
	{
	    warn "opendir($dotdots): $!";
	    return '';
	}
	unless (@cst = stat($dotdots))
	{
	    warn "stat($dotdots): $!";
	    closedir(getcwd'PARENT);					#');
	    return '';
	}
	if ($pst[$[] == $cst[$[] && $pst[$[ + 1] == $cst[$[ + 1])
	{
	    $dir = '';
	}
	else
	{
	    do
	    {
		unless ($dir = readdir(getcwd'PARENT))			#'))
		{
		    warn "readdir($dotdots): $!";
		    closedir(getcwd'PARENT);				#');
		    return '';
		}
		unless (@tst = lstat("$dotdots/$dir"))
		{
		    warn "lstat($dotdots/$dir): $!";
		    closedir(getcwd'PARENT);				#');
		    return '';
		}
	    }
	    while ($dir eq '.' || $dir eq '..' || $tst[$[] != $pst[$[] ||
		   $tst[$[ + 1] != $pst[$[ + 1]);
	}
	$cwd = "$dir/$cwd";
	closedir(getcwd'PARENT);					#');
    } while ($dir);
    chop($cwd);
    $cwd;
}

1;
