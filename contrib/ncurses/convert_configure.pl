extproc perl -S -w

# The converted script is written to stdout, so run this script as
#    convert_configure configure > configure.cmd
#
# When the converted script runs, it expects that /tmp dir is
# available (so we create it).
#
# run the result like this:
#    .\configure
;

mkdir '/tmp', 0777 unless -d '/tmp';

print <<EOF;
extproc sh

# Make sensible defaults:
CC="gcc -Zexe"
export CC
#GCCOPT="$GCCOPT -Zexe"
#export GCCOPT
CONFIG_SHELL=sh

EOF

$checking_path = 0;

while (<>) {
  if (/for\s+(\w+)\s+in\s*\$PATH\s*;/) {
    $checking_path = 1;
    $varname = $1;
    $subst= <<EOS
$varname="`echo -E \\"\$$varname\\" | tr \\\\\\\\\\\\\\\\ / `"
EOS
  } 
  $checking_path = 0 if /^\s*done\s*$/;
  # We want to create an extra line like this one:
#   ac_dir="`echo -E \"$ac_dir\" | tr \\\\\\\\ / `"
  s{^((\s*)if\s+test)\s*-f\s*(\$$varname/\S+)\s*;}
    {$2$subst$1 -f $3 -o -f $3.exe ;}
      if $checking_path;	# Checking for executables
  s/^host=NONE$/host=os2/;	# Make default host
  s/"\$\{IFS}:"$/"\${IFS};"/;	# Fix IFS line
  s/\btest\s+-s\s+conftest\b/test -f conftest/g; # Fix exe test
  # This one is needed for curses:
  s/host=`\$ac_config_sub \$host_alias`/$&\nif test -z "$host"; then host=\$host_alias; fi/;
  s,/bin/sh(?![/\w]),sh,g;
  print;
}

__END__

Changes: 98/11 : support check for executables in ncurses.
