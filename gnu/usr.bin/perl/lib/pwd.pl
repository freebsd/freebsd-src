;# pwd.pl - keeps track of current working directory in PWD environment var
;#
;# $RCSfile: pwd.pl,v $$Revision: 1.1.1.1 $$Date: 1993/08/23 21:29:52 $
;#
;# $Log: pwd.pl,v $
# Revision 1.1.1.1  1993/08/23  21:29:52  nate
# PERL!
#
;# Revision 4.0.1.1  92/06/08  13:45:22  lwall
;# patch20: support added to pwd.pl to strip automounter crud
;# 
;# Revision 4.0  91/03/20  01:26:03  lwall
;# 4.0 baseline.
;# 
;# Revision 3.0.1.2  91/01/11  18:09:24  lwall
;# patch42: some .pl files were missing their trailing 1;
;# 
;# Revision 3.0.1.1  90/08/09  04:01:24  lwall
;# patch19: Initial revision
;# 
;#
;# Usage:
;#	require "pwd.pl";
;#	&initpwd;
;#	...
;#	&chdir($newdir);

package pwd;

sub main'initpwd {
    if ($ENV{'PWD'}) {
	local($dd,$di) = stat('.');
	local($pd,$pi) = stat($ENV{'PWD'});
	if ($di != $pi || $dd != $pd) {
	    chop($ENV{'PWD'} = `pwd`);
	}
    }
    else {
	chop($ENV{'PWD'} = `pwd`);
    }
    if ($ENV{'PWD'} =~ m|(/[^/]+(/[^/]+/[^/]+))(.*)|) {
	local($pd,$pi) = stat($2);
	local($dd,$di) = stat($1);
	if ($di == $pi && $dd == $pd) {
	    $ENV{'PWD'}="$2$3";
	}
    }
}

sub main'chdir {
    local($newdir) = shift;
    if (chdir $newdir) {
	if ($newdir =~ m#^/#) {
	    $ENV{'PWD'} = $newdir;
	}
	else {
	    local(@curdir) = split(m#/#,$ENV{'PWD'});
	    @curdir = '' unless @curdir;
	    foreach $component (split(m#/#, $newdir)) {
		next if $component eq '.';
		pop(@curdir),next if $component eq '..';
		push(@curdir,$component);
	    }
	    $ENV{'PWD'} = join('/',@curdir) || '/';
	}
    }
    else {
	0;
    }
}

1;
