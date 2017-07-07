#!perl -w

#use strict;

sub commandandcontrol {
    local ($configdefault, $bIgnoreCmdlineConfig)   = @_;
    local $OPT = {foo => 'bar'};

    Getopt::Long::Configure('no_bundling', 'no_auto_abbrev',
           'no_getopt_compat', 'require_order',
           'ignore_case', 'pass_through',
           'prefix_pattern=(--|-|\+|\/)'
           );
    GetOptions($OPT,
           'help|h|?',
           'cvstag|c:s',
           'svntag|s:s',
           'svnbranch|b:s',
           'src|r:s',
           'out|o:s',
           'debug|d',
           'nodebug',
           'config|f=s',
           'logfile|l:s',
           'nolog',
           'repository:s',
           'username|u:s',
           'verbose|v',
           'vverbose',
           'make!',
           'clean',
           'package!',
           'sign!',
           );

    if ( $OPT->{help} ) {
        usage();
        exit(0);
        }
        
    delete $OPT->{foo};

    local $argvsize = @ARGV;
    if ($argvsize > 0) {
        print "Error -- invalid argument:  $ARGV[0]\n";
        usage();
        die;
        }
    # The first time C&C is called, it is OK to override the default (./bkwconfig.xml)
    #   with a value from the command line.
    # The second time C&C is called, the repository has been updated and C&C will be passed
    #   <src>/pismere/athena/auth/krb5/windows/build/bkwconfig.xml.  That value MUST be used.
    if ($bIgnoreCmdlineConfig)      {$OPT->{config} = $configdefault;}
    elsif (! exists $OPT->{config}) {$OPT->{config} = $configdefault;}

    my $configfile      = $OPT->{config};
    my $bOutputCleaned  = 0;

    print "Info -- Reading configuration from $configfile.\n";

    # Get configuration file:
    local $xml = new XML::Simple();
    my $config = $xml->XMLin($configfile);
    # Set up convenience variables:
    local $odr  = $config->{Config};    ## Options, directories, repository, environment.

#while ($v = each %$OPT) {print "$v: $OPT->{$v}\n";}

    # Scan the configuration for switch definitions:
    while (($sw, $val) = each %$odr) {
        next if (! exists $val->{def}); ## ?? Should always exist.

        # Set/clear environment variables:
        if ($val->{env}) {
            if ($val->{def})    {$ENV{$sw}   = (exists $val->{value}) ? $val->{value} : 1; }
            else                {delete $ENV{$sw};  }
            }

        # If the switch is in the command line, override the stored value:
        if (exists $OPT->{$sw}) {
            if (exists $val->{value}) {
                $val->{value}   = $OPT->{$sw};  
                $val->{def}     = 1;
                }
            else {
                $val->{def}   = $OPT->{$sw};    ## If no<switch>, value will be zero.
                }
            }
        # If the switch can be negated, test that, too:
        if ( ! ($val->{def} =~ /A/)) {
            local $nosw = "no".$sw;
            if (exists $OPT->{$nosw}) {
                $val->{def} = 0;
                }
            }
    
        # For any switch definition with fixed values ("options"), validate:
        if (exists $val->{options}) {
            local $bValid   = 0;
            # options can be like value1|syn1 value2|syn2|syn3
            foreach $option (split(/ /, $val->{options})) {
                local $bFirst   = 1;
                local $sFirst;
                foreach $opt (split(/\|/, $option)) {
                    # opt will be like value2, syn2, syn3
                    if ($bFirst) {
                        $sFirst = $opt; ## Remember the full name of the option.
                        $bFirst = 0;
                        }
                    if ($val->{value} =~ /$opt/i) {
                        $val->{value} = $sFirst;    ## Save the full name.
                        $bValid = 1;
                        }
                    }
                }
            if (! $bValid) {
                print "Fatal -- invalid $sw value $val->{value}.  Possible values are $val->{options}.\n";
                usage();
                die;
                }
            }
        }

    # Don't allow /svntag and /svnbranch simultaneously:
    if ( (length $odr->{svntag}->{value} > 0)   && 
         (length $odr->{svnbranch}->{value} > 0) ) {
        die "Fatal -- Can't specify both /SVNTAG and /SVNBRANCH.";
        }

    return $config;
    }
    

sub usage {
    print <<USAGE;
Usage: $0 [options] NMAKE-options

  Options are case insensitive.

  Options:
    /help /?           usage information (what you now see).
    /config /f path    Path to config file.  Default is bkwconfig.xml.
    /srcdir /r dir     Source directory to use.  Should contain 
                       pismere/athena.  If cvstag or svntag is null, 
                       the directory should be prepopulated.
    /outdir /o dir     Directory to be created where build results will go
    /repository checkout | co \\  What repository action to take.
                update   | up  ) Options are to checkout, update or 
                skip          /  take no action [skip].
    /username /u name  username used to access svn if checking out.
    /cvstag /c tag     use -r <tag> in cvs command
    /svnbranch /b tag  use /branches/<tag> instead of /trunk.
    /svntag /s tag     use /tags/<tag> instead of /trunk.
    /debug /d          Do debug make instead of release make.
    /[no]make          Control the make step.
    /clean             Build clean target.
    /[no]package       Control the packaging step.
    /[no]sign          Control signing of executable files.
    /verbose /v        Debug mode - verbose output.
    /logfile /l path   Where to write output.  Default is bkw.pl.log.
    /nolog             Don't save output.
  Other:
    NMAKE-options      any options you want to pass to NMAKE, which can be:
                       (note: /nologo is always used)

USAGE
    system("$MAKE /?");
    }

return 1;