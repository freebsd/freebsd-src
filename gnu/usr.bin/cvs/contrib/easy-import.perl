#!/usr/bin/perl
#
# Support for importing a source collection into CVS.
# Trys to prevent the user from the most common pitfalls (like creating
# new top-level repositories or second-level areas accidentally), and
# cares to do some of the `dirty' work like maintaining the modules
# database accordingly.
#
# Written by Jörg Wunsch, 95/03/07, and placed in the public domain.
#

require "complete.pl";


sub lsdir
{
    # find all subdirectories under @_
    # ignore all CVS entries, dot entries, and non-directories

    local($base) = @_;
    local(@ls, @rv, $fname);

    opendir(DIR, $base) || die "Cannot find dir $base.\n";

    @ls = readdir(DIR);
    closedir(DIR);

    @rv = ();

    foreach $fname (@ls) {
	next if $fname =~ /^CVS/ || $fname eq "Attic"
	    || $fname =~ /^\./ || ! -d "$base/$fname";
	@rv = (@rv, $fname);
    }

    return sort(@rv);
}


sub contains
{
    # look if the first parameter is contained in the list following it
    local($item, @list) = @_;
    local($found, $i);

    $found = 0;
    foreach $i (@list) {
	return 1 if $i eq $item;
    }
    return 0;
}



sub term_init
{
    # first, get some terminal attributes

    # try bold mode first
    $so = `tput md`; $se = `tput me`;

    # if no bold mode available, use standout mode
    if ($so eq "") {
	$so = `tput so`; $se = `tput se`;
    }

    # try if we can underscore
    $us = `tput us`; $ue = `tput ue`;
    # if we don't have it available, or same as bold/standout, disable it
    if ($us eq "" || $us eq $so) {
	$us = $ue = "";
    }

    # look how many columns we've got
    if($ENV{'COLUMNS'} ne "") {
	$columns = $ENV{'COLUMNS'};
    } elsif(-t STDIN) {		# if we operate on a terminal...
	local($word, $tmp);

	open(STTY, "stty -a|");
	$_ = <STTY>;		# try getting the tty win structure value
	close(STTY);
	chop;
	$columns = 0;
	foreach $word (split) {
	    $columns = $tmp if $word eq "columns;"; # the number preceding
	    $tmp = $word;
	}
    } else {
	$columns = 80;
    }
    # sanity
    $columns = 80 unless $columns >= 5;
}


sub list
{
    # pretty-print a list
    # imports: global variable $columns
    local(@items) = @_;
    local($longest,$i,$item,$cols,$width);

    # find the longest item
    $longest = 0;
    foreach $item (@items) {
	$i = length($item);
	$longest = $i if $longest < $i;
    }
    $width = $longest + 1;
    $cols = int($columns / $width);

    $i = 0;
    foreach $item (@items) {
	print $item;
	if(++$i == $cols) {
	    $i = 0; print "\n";
	} else {
	    print ' ' x ($width - length($item));
	}
    }
    print "\n" unless $i == 0;
}

sub cvs_init
{
    # get the CVS repository(s)

    die "You need to have the \$CVSROOT variable set.\n"
	unless $ENV{'CVSROOT'} ne "";

    # get the list of available repositories
    $cvsroot = $ENV{'CVSROOT'};
    @reps = &lsdir($cvsroot);
}


sub lsmodules
{
    # list all known CVS modules
    local(@rv, $mname, $_);

    @rv = ();

    open(CVS, "cvs co -c|");
    while($_ = <CVS>) {
	chop;
	($mname) = split;
	next if $mname eq "";
	@rv = (@rv, $mname);
    }
    close(CVS);

    return @rv;
}


sub checktag
{
    # check a given string for tag rules
    local($s) = @_;
    return 0 if($s !~ /^[A-Za-z][A-Za-z0-9_]*$/);

    return 1;
}


&term_init;
&cvs_init;

print "${so}Available repositories:${se}\n";
&list(@reps);

$selected =
    &Complete("Enter repository (<TAB>=complete, ^D=show): ",
	      @reps);

die "\aYou cannot create new repositories with this script.\n"
    unless &contains($selected, @reps);

$rep = $selected;

print "\n${so}Selected repository:${se} ${us}$rep${ue}\n";


@areas = &lsdir("$cvsroot/$rep");

print "${so}Existent areas in this repository:${se}\n";
&list(@areas);

# the following kludge prevents the Complete package from starting
# over with the string just selected; Complete should better provide
# some reinitialize method
$Complete'return = "";   $Complete'r = 0;

$selected =
    &Complete("Enter area name (<TAB>=complete, ^D=show): ",
	      @areas);

print "\a${us}Warning: this will create a new area.${ue}\n"
    unless &contains($selected, @areas);

$area = "$rep/$selected";

print "\n${so}[Working on:${se} ${us}$area${ue}${so}]${se}\n";

for(;;) {
    $| = 1;
    print "${so}Enter the module path:${se} $area/";
    $| = 0;
    $modpath = <>;
    chop $modpath;
    if ($modpath eq "") {
	print "\a${us}You cannot use an empty module path.${ue}\n";
	next;
    }
    last if ! -d "$cvsroot/$area/$modpath";
    print "\a${us}This module path does already exist; " .
	"choose another one.${ue}\n";
}


@newdirs = ();
$dir1 = "$cvsroot/$area";
$dir2 = "$area";

@newdirs = (@newdirs, "$dir2") if ! -d $dir1;

foreach $ele (split(/\//, $modpath)) {
    $dir1 = "$dir1/$ele";
    $dir2 = "$dir2/$ele";
    @newdirs = (@newdirs, "$dir2") if ! -d $dir1;
}

print "${so}You're going to create the following new directories:${se}\n";

&list(@newdirs);

@cvsmods = &lsmodules();

for(;;) {
    $| = 1;
    print "${so}Gimme the module name:${se} ";
    $| = 0;
    $modname = <>;
    chop $modname;
    if ($modname eq "") {
	print "\a${us}You cannot use an empty module name.${ue}\n";
	next;
    }
    last if !&contains($modname, @cvsmods);
    print "\a${us}This module name does already exist; " .
	"choose another one.${ue}\n";
}


for(;;) {
    $| = 1;
    print "${so}Enter a \`vendor\' tag (e. g. the authors ID):${se} ";
    $| = 0;
    $vtag = <>;
    chop $vtag;
    last if &checktag($vtag);
    print "\a${us}Valid tags must match the regexp " .
	"^[A-Za-z][A-Za-z0-9_]*\$.${ue}\n";
}

for(;;) {
    $| = 1;
    print "${so}Enter a \`release\' tag (e. g. the version #):${se} ";
    $| = 0;
    $rtag = <>;
    chop $rtag;
    last if &checktag($rtag);
    print "\a${us}Valid tags must match the regexp " .
	"^[A-Za-z][A-Za-z0-9_]*\$.${ue}\n";
}


$| = 1;
print "${so}This is your last chance to interrupt, " .
    "hit <return> to go on:${se} ";
$| = 0;
<>;

$mod = "";
foreach $tmp (@cvsmods) {
    if($tmp gt $modname) {
	$mod = $tmp;
	last;
    }
}

if($mod eq "") {
    # we are going to append our module
    $cmd = "\$\na\n";
} else {
    # we can insert it
    $cmd = "/^${mod}[ \t]/\ni\n";
}

print "${so}Checking out the modules database...${se}\n";
system("cvs co modules") && die "${us}failed.\n${ue}";

print "${so}Inserting new module...${se}\n";
open(ED, "|ed modules/modules") || die "${us}Cannot start ed${ue}\n";
print(ED "${cmd}${modname}" . ' ' x (32 - length($modname)) .
      "$area/${modpath}\n.\nw\nq\n");
close(ED);

print "${so}Commiting new modules database...${se}\n";
system("cvs commit -m \"  ${modname} --> $area/${modpath}\" modules")
    && die "Commit failed\n";

system("cvs release -dQ modules");

print "${so}Importing source.  Enter a commit message in the editor.${se}\n";

system("cvs import $area/$modpath $vtag $rtag");

print "${so}You are done now.  Go to a different directory, perform a${se}\n".
    "${us}cvs co ${modname}${ue} ${so}command, and see if your new module" .
    " builds ok.${se}\n";

