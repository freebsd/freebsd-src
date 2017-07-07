#!perl -w

#use strict;
use Data::Dumper;

sub signFiles {
    local ($signing, $config)   = @_;
    local $exprs    = $signing->{FilePatterns}->{value};
    local $template = $signing->{CommandTemplate}->{value};
    # Use Unix find instead of Windows find.  Save PATH so we can restore it when we're done:
    local $savedPATH= $ENV{PATH};
    $ENV{PATH}      = $config->{Config}->{unixfind}->{value}.";".$savedPATH;
    foreach $expr (split(" ", $exprs)) {            ## exprs is something like "*.exe *.dll"
        local $cmd  = "find . -iname \"$expr\"";
        local $list = `$cmd`;                       ## $list is files matching *.exe, for example.
        foreach $target (split("\n", $list)) {
            $target =~ s|/|\\|g;                    ## Flip path separators from unix-style to windows-style.
            local $template2    = $template;
            $template2          =~ s/%filename%/$target/;
            print "Info -- Signing $target\n" if ($verbose);
            !system("$template2") or die "Fatal -- Error signing $target.";
            }
        }
        $ENV{PATH} = $savedPATH;
    }

return 1;