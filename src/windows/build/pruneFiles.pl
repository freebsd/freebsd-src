#!perl -w

#use strict;
use Data::Dumper;

sub pruneFiles {
    local ($xml, $config)   = @_;
    local $prunes       = $xml->{Prunes};
    if (! $prunes) {return 0;}

    # Use Unix find instead of Windows find.  Save PATH so we can restore it when we're done:
    local $savedPATH    = $ENV{PATH};
    $ENV{PATH}          = $config->{Config}->{unixfind}->{value}.";".$savedPATH;
    print "Info -- Processing prunes in ".`cd`."\n"     if ($verbose);
    local $pru          = $prunes->{Prune};
    local $files        = "( ";
    local $bFirst       = 1;
    while (($key, $val) = each %$pru) {
        local $flags    = $val->{flags};
        $flags          = "" if (!$flags);
        if (!$bFirst)   {$files .= " -or ";}
        $bFirst         = 0;
        $files          .= "-".$flags."name $key";
        print "Info -- Looking for filenames matching $key\n"   if ($verbose);
        }
    $files              .= " )";
    local $list = `find . $files`;
    if (length($list) >   1) {
        print "Info -- Pruning $list\n" if ($verbose);
        ! system("rm -rf $list")              or die "Unable to prune $list";
        }

    $ENV{PATH} = $savedPATH;
    }

return 1;
