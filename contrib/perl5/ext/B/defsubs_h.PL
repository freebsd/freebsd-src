# Do not remove the following line; MakeMaker relies on it to identify
# this file as a template for defsubs.h
# Extracting defsubs.h (with variable substitutions)
#!perl
my ($out) = __FILE__ =~ /(^.*)\.PL/i;
$out =~ s/_h$/.h/;
open(OUT,">$out") || die "Cannot open $file:$!";
print "Extracting $out...\n";
foreach my $const (qw(AVf_REAL 
		      HEf_SVKEY
                      SVf_IOK SVf_IVisUV SVf_NOK SVf_POK 
		      SVf_ROK SVp_IOK SVp_POK ))
 {
  doconst($const);
 }
foreach my $file (qw(op.h cop.h))
 {
  open(OPH,"../../$file") || die "Cannot open ../../$file:$!";
  while (<OPH>)
   {  
    doconst($1) if (/#define\s+(\w+)\s+([\(\)\|\dx]+)\s*(?:$|\/\*)/);
   }  
  close(OPH);
 }
close(OUT);
               
sub doconst
{
 my $sym = shift;
 my $l = length($sym);
 print OUT <<"END";
 newCONSTSUB(stash,"$sym",newSViv($sym)); 
 av_push(export_ok,newSVpvn("$sym",$l));
END
}
