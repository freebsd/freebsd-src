#!/usr/local/bin/perl -w

BEGIN {
    chdir('t') if -d 't';
    @INC = '../lib';
}

# Test ability to retrieve HTTP request info
######################### We start with some black magic to print on failure.
use lib '../blib/lib','../blib/arch';

BEGIN {$| = 1; print "1..24\n"; }
END {print "not ok 1\n" unless $loaded;}
use CGI (':standard','-no_debug','*h3','start_table');
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

my $CRLF = "\015\012";
if ($^O eq 'VMS') { 
  $CRLF = "\n";  # via web server carriage is inserted automatically
}
if (ord("\t") != 9) { # EBCDIC?
  $CRLF = "\r\n";
}


# util
sub test {
    local($^W) = 0;
    my($num, $true,$msg) = @_;
    print($true ? "ok $num\n" : "not ok $num $msg\n");
}

# all the automatic tags
test(2,h1() eq '<h1 />',"single tag");
test(3,h1('fred') eq '<h1>fred</h1>',"open/close tag");
test(4,h1('fred','agnes','maura') eq '<h1>fred agnes maura</h1>',"open/close tag multiple");
test(5,h1({-align=>'CENTER'},'fred') eq '<h1 align="CENTER">fred</h1>',"open/close tag with attribute");
test(6,h1({-align=>undef},'fred') eq '<h1 align>fred</h1>',"open/close tag with orphan attribute");
test(7,h1({-align=>'CENTER'},['fred','agnes']) eq 
     '<h1 align="CENTER">fred</h1> <h1 align="CENTER">agnes</h1>',
     "distributive tag with attribute");
{
    local($") = '-'; 
    test(8,h1('fred','agnes','maura') eq '<h1>fred-agnes-maura</h1>',"open/close tag \$\" interpolation");
}
test(9,header() eq "Content-Type: text/html; charset=ISO-8859-1${CRLF}${CRLF}","header()");
test(10,header(-type=>'image/gif') eq "Content-Type: image/gif${CRLF}${CRLF}","header()");
test(11,header(-type=>'image/gif',-status=>'500 Sucks') eq "Status: 500 Sucks${CRLF}Content-Type: image/gif${CRLF}${CRLF}","header()");
test(12,header(-nph=>1) =~ m!HTTP/1.0 200 OK${CRLF}Server: cmdline${CRLF}Date:.+${CRLF}Content-Type: text/html; charset=ISO-8859-1${CRLF}${CRLF}!,"header()");
test(13,start_html() ."\n" eq <<END,"start_html()");
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html
	PUBLIC "-//W3C//DTD XHTML Basic 1.0//EN"
	"http://www.w3.org/TR/xhtml-basic/xhtml-basic10.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en-US"><head><title>Untitled Document</title>
</head><body>
END
    ;
test(14,start_html(-dtd=>"-//IETF//DTD HTML 3.2//FR") ."\n" eq <<END,"start_html()");
<!DOCTYPE html
	PUBLIC "-//IETF//DTD HTML 3.2//FR">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en-US"><head><title>Untitled Document</title>
</head><body>
END
    ;
test(15,start_html(-Title=>'The world of foo') ."\n" eq <<END,"start_html()");
<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE html
	PUBLIC "-//W3C//DTD XHTML Basic 1.0//EN"
	"http://www.w3.org/TR/xhtml-basic/xhtml-basic10.dtd">
<html xmlns="http://www.w3.org/1999/xhtml" lang="en-US"><head><title>The world of foo</title>
</head><body>
END
    ;
test(16,($cookie=cookie(-name=>'fred',-value=>['chocolate','chip'],-path=>'/')) eq 'fred=chocolate&chip; path=/',"cookie()");
my $h = header(-Cookie=>$cookie);
test(17,$h =~ m!^Set-Cookie: fred=chocolate&chip\; path=/${CRLF}Date:.*${CRLF}Content-Type: text/html; charset=ISO-8859-1${CRLF}${CRLF}!s, 
  "header(-cookie)");
test(18,start_h3 eq '<h3>');
test(19,end_h3 eq '</h3>');
test(20,start_table({-border=>undef}) eq '<table border>');
test(21,h1(escapeHTML("this is <not> \x8bright\x9b")) eq '<h1>this is &lt;not&gt; &#139;right&#155;</h1>');
charset('utf-8');
if (ord("\t") == 9) {
test(22,h1(escapeHTML("this is <not> \x8bright\x9b")) eq '<h1>this is &lt;not&gt; ‹right›</h1>');
}
else {
test(22,h1(escapeHTML("this is <not> \x8bright\x9b")) eq '<h1>this is &lt;not&gt; »rightº</h1>');
}
test(23,i(p('hello there')) eq '<i><p>hello there</p></i>');
my $q = new CGI;
test(24,$q->h1('hi') eq '<h1>hi</h1>');
