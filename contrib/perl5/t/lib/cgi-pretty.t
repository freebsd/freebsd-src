#!/usr/local/bin/perl -w

BEGIN {
    chdir('t') if -d 't';
    @INC = '../lib';
}

# Test ability to retrieve HTTP request info
######################### We start with some black magic to print on failure.
use lib '../blib/lib','../blib/arch';

BEGIN {$| = 1; print "1..5\n"; }
END {print "not ok 1\n" unless $loaded;}
use CGI::Pretty (':standard','-no_debug','*h3','start_table');
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# util
sub test {
    local($^W) = 0;
    my($num, $true,$msg) = @_;
    print($true ? "ok $num\n" : "not ok $num $msg\n");
}

# all the automatic tags
test(2,h1() eq '<h1>',"single tag");
test(3,ol(li('fred'),li('ethel')) eq "<ol>\n\t<li>\n\t\tfred\n\t</li>\n\t <li>\n\t\tethel\n\t</li>\n</ol>\n","basic indentation");
test(4,p('hi',pre('there'),'frog') eq 
'<p>
	hi <pre>there</pre>
	 frog
</p>
',"<pre> tags");
test(5,p('hi',a({-href=>'frog'},'there'),'frog') eq 
'<p>
	hi <a href="frog">there</a>
	 frog
</p>
',"as-is");
