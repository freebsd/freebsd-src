#!perl -w

#use strict;
use XML::Simple;
use Data::Dumper;

sub copyFiles {
    local ($xml, $config)   = @_;
    local @odr              = $config->{Config};
    local @files            = $xml->{Files};
    # Check for includes:
    if (exists $xml->{Files}->{Include}->{path}) {
        my $includepath    = $xml->{Files}->{Include}->{path};
        print "Info -- Including files from $includepath\n";
        my $savedDir = `cd`;
        $savedDir =~ s/\n//g;
        chdir $originalDir;                         ## Includes are relative to where we were invoked.
        print "Info -- chdir to ".`cd`."\n"         if ($verbose);
        my $tmp    = new XML::Simple;
        my $includeXML = $tmp->XMLin($includepath);
        chdir $savedDir;
        print "Info -- chdir to ".`cd`."\n"         if ($verbose);

        local $i = 0;
        while ($includeXML->{File}[$i]) {           ## Copy File entries from includeXML.
            $files[0]->{File}[++$#{$files[0]->{File}}] = $includeXML->{File}[$i];        
            $i++;
            }
        delete $files->{Include};
        }
    ##++ Set up path substitution variables for use inside the copy loop:
    # A path can contain a variable part, which will be handled here.  If the variable part is 
    # the Always or BuildDependent tag, then the variable will be changed to the 
    # build-type-dependent PathFragment.
    # If the variable part is the IgnoreTag, then the file will not be copied.
    # If the variable part is %filestem%, it will be replaced with Config->FileStem->name.
    my ($PathFragment, $BuildDependentTag, $IgnoreTag, $FileStemFragment, $fromRoot, $toRoot); 
    my $bPathTags    = (exists $xml->{Config}->{DebugArea}) && (exists $xml->{Config}->{ReleaseArea});
    my $bFileStem    = (exists $xml->{Config}->{FileStem});
    
    if ($odr->{debug}->{def}) {   ## Debug build tags:
        $PathFragment       = $xml->{Config}->{DebugArea}->{value};
        $BuildDependentTag  = $xml->{Config}->{DebugTag}->{value};
        $IgnoreTag          = $xml->{Config}->{ReleaseTag}->{value};
        }
    else {                                  ## Release build tags:
        $PathFragment       = $xml->{Config}->{ReleaseArea}->{value};
        $BuildDependentTag  = $xml->{Config}->{ReleaseTag}->{value};
        $IgnoreTag          = $xml->{Config}->{DebugTag}->{value};
        }            
    my $AlwaysTag           = $xml->{Config}->{AlwaysTag}->{value};
    $FileStemFragment       = $xml->{Config}->{FileStem}->{name};    
    $fromRoot               = $xml->{Config}->{From}->{root};    
    $toRoot                 = $xml->{Config}->{To}->{root};    
    ##-- Set up path substitution variables for use inside the copy loop.
    # For each file in the file list:
    #  Substitute any variable parts of the path name.
    #  Handle wildcards
    #  Copy

    local $i    = 0;
    my $bOldDot = 1;
    my $bDot    = 0;
    while ($files[0]->{File}[$i]) {

        my ($name, $newname, $from, $to, $file);
        $file   = $files[0]->{File}->[$i];
        $name   = $file->{name};
        if (exists $file->{newname})    {$newname = $file->{newname};}
        else                            {$newname = $name;}
        if ($name && (! exists $file->{ignore})) {      ## Ignore or process this entry?
            $from   = "$fromRoot\\$file->{from}\\$name";
            $to     = "$toRoot\\$file->{to}\\$newname";
            # Copy this file?  Check for ignore tag [debug-only in release mode or vice versa].
            if ( $bPathTags || $bFileStem || (index($from.$to, $IgnoreTag) <0) ) {  
                if ($bPathTags) {                                   ## Apply PathTag substitutions:
                    $from   =~ s/$AlwaysTag/$PathFragment/g;
                    $to     =~ s/$AlwaysTag/$PathFragment/g;
                    $from   =~ s/$BuildDependentTag/$PathFragment/g;
                    $to     =~ s/$BuildDependentTag/$PathFragment/g;
                    }
                if ($bFileStem) {                                   ## FileStem substitution?
                    $from   =~ s/%filestem%/$FileStemFragment/g;
                    $to     =~ s/%filestem%/$FileStemFragment/g;
                    }        
                # %-DEBUG% substitution:
                local $DebugFragment    = ($odr->{debug}->{def}) ? "-DEBUG" : "";
                $from       =~ s/%\-DEBUG%/$DebugFragment/g;
                $to         =~ s/%\-DEBUG%/$DebugFragment/g;
                $to         =~ s/\*.*//;                ## Truncate to path before any wildcard

                my $bCopyOK     = 1;
                my $fromcheck   = $from;
                my $bRequired   = ! (exists $file->{notrequired});
                if ($name =~ /\*/) {                    ## Wildcard case
                    $fromcheck =~ s/\*.*//;
                    if ($bRequired && (! -d $fromcheck)) {
                        if ($bDot) {print "\n";}
                        die "Fatal -- Can't find $fromcheck";
                        }
                    $bCopyOK = !system("echo D | xcopy /D /F /Y /S  $from $to > a.tmp 2>NUL");
                    }
                else {                                  ## Specific file case
                    if ($bRequired && (! -e $fromcheck)) {
                        if ($bDot) {print "\n";}
                        die "Fatal -- Can't find $fromcheck";
                        }
                    $bCopyOK    = !system("echo F | xcopy /D /F /Y $from $to > a.tmp 2>NUL");
                    }

                if ($bCopyOK) {                         ## xcopy OK - show progress
                    # To show progress when files aren't copied, print a string of dots.
                    open(MYINPUTFILE, "<a.tmp");
                    my(@lines) = <MYINPUTFILE>;
                    foreach $line (@lines) { 
                        $bDot = ($line =~ /^0/);
                        }
                    close(MYINPUTFILE);
                    if (!$bDot && $bOldDot) {print "\n";}
                    if ($bDot) {print "."; STDOUT->flush;}
                    else {print "$from copied to $to\n";}
                    $bOldDot = $bDot;
                    }
                else {                                  ## xcopy failed
                    if (!exists $file->{notrequired}) {
                        if ($bDot) {print "\n";}
                        die "Fatal -- Copy of $from to $to failed";
                        }
                    }                                   ## End xcopy succeed or fail
                }                                       ## End not dummy entry nor ignored
            }
        $i++;
        }
    if ($bDot) {print "\n";}
    }

return 1;
