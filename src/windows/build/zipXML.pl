#!perl -w

#use strict;
require "makeZip.pl";

use Data::Dumper;

sub zipXML {
    local ($xml, $config)   = @_;
    my $zipsXML = $xml->{Zips};
    if (! $zipsXML) {return 0;}

    local $i    = 0;
    while ($zipsXML->{Zip}[$i]) {
        local $zip = $zipsXML->{Zip}[$i];
        makeZip($zip, $config)  if (exists $zip->{name});       ## Ignore dummy entry.
        $i++;                    
        }                                       ## End zip in xml.
    }
    
return 1;
