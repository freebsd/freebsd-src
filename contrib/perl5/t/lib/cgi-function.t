#!/usr/local/bin/perl -w

BEGIN {
    chdir('t') if -d 't';
    @INC = '../lib';
}

# Test ability to retrieve HTTP request info
######################### We start with some black magic to print on failure.
use lib '../blib/lib','../blib/arch';

BEGIN {$| = 1; print "1..27\n"; }
END {print "not ok 1\n" unless $loaded;}
use Config;
use CGI (':standard','keywords');
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# util
sub test {
    local($^W) = 0;
    my($num, $true,$msg) = @_;
    print($true ? "ok $num\n" : "not ok $num $msg\n");
}

my $CRLF = "\015\012";

# A peculiarity of sending "\n" through MBX|Socket|web-server on VMS 
# is that a CR character gets inserted automatically in the web server 
# case but not internal to perl's double quoted strings "\n".  This
# test would need to be modified to use the "\015\012" on VMS if it
# were actually run through a web server.
# Thanks to Peter Prymmer for this

if ($^O eq 'VMS') { $CRLF = "\n"; }

# Web servers on EBCDIC hosts are typically set up to do an EBCDIC -> ASCII
# translation hence CRLF is used as \r\n within CGI.pm on such machines.

if (ord("\t") != 9) { $CRLF = "\r\n"; }

# Set up a CGI environment
$ENV{REQUEST_METHOD}='GET';
$ENV{QUERY_STRING}  ='game=chess&game=checkers&weather=dull';
$ENV{PATH_INFO}     ='/somewhere/else';
$ENV{PATH_TRANSLATED} ='/usr/local/somewhere/else';
$ENV{SCRIPT_NAME}   ='/cgi-bin/foo.cgi';
$ENV{SERVER_PROTOCOL} = 'HTTP/1.0';
$ENV{SERVER_PORT} = 8080;
$ENV{SERVER_NAME} = 'the.good.ship.lollypop.com';
$ENV{HTTP_LOVE} = 'true';

test(2,request_method() eq 'GET',"CGI::request_method()");
test(3,query_string() eq 'game=chess;game=checkers;weather=dull',"CGI::query_string()");
test(4,param() == 2,"CGI::param()");
test(5,join(' ',sort {$a cmp $b} param()) eq 'game weather',"CGI::param()");
test(6,param('game') eq 'chess',"CGI::param()");
test(7,param('weather') eq 'dull',"CGI::param()");
test(8,join(' ',param('game')) eq 'chess checkers',"CGI::param()");
test(9,param(-name=>'foo',-value=>'bar'),'CGI::param() put');
test(10,param(-name=>'foo') eq 'bar','CGI::param() get');
test(11,query_string() eq 'game=chess;game=checkers;weather=dull;foo=bar',"CGI::query_string() redux");
test(12,http('love') eq 'true',"CGI::http()");
test(13,script_name() eq '/cgi-bin/foo.cgi',"CGI::script_name()");
test(14,url() eq 'http://the.good.ship.lollypop.com:8080/cgi-bin/foo.cgi',"CGI::url()");
test(15,self_url() eq 
     'http://the.good.ship.lollypop.com:8080/cgi-bin/foo.cgi/somewhere/else?game=chess;game=checkers;weather=dull;foo=bar',
     "CGI::url()");
test(16,url(-absolute=>1) eq '/cgi-bin/foo.cgi','CGI::url(-absolute=>1)');
test(17,url(-relative=>1) eq 'foo.cgi','CGI::url(-relative=>1)');
test(18,url(-relative=>1,-path=>1) eq 'foo.cgi/somewhere/else','CGI::url(-relative=>1,-path=>1)');
test(19,url(-relative=>1,-path=>1,-query=>1) eq 
     'foo.cgi/somewhere/else?game=chess;game=checkers;weather=dull;foo=bar',
     'CGI::url(-relative=>1,-path=>1,-query=>1)');
Delete('foo');
test(20,!param('foo'),'CGI::delete()');

CGI::_reset_globals();
$ENV{QUERY_STRING}='mary+had+a+little+lamb';
test(21,join(' ',keywords()) eq 'mary had a little lamb','CGI::keywords');
test(22,join(' ',param('keywords')) eq 'mary had a little lamb','CGI::keywords');

CGI::_reset_globals;
if ($Config{d_fork}) {
  $test_string = 'game=soccer&game=baseball&weather=nice';
  $ENV{REQUEST_METHOD}='POST';
  $ENV{CONTENT_LENGTH}=length($test_string);
  $ENV{QUERY_STRING}='big_balls=basketball&small_balls=golf';
  if (open(CHILD,"|-")) {  # cparent
    print CHILD $test_string;
    close CHILD;
    exit 0;
  }
  # at this point, we're in a new (child) process
  test(23,param('weather') eq 'nice',"CGI::param() from POST");
  test(24,(url_param('big_balls') eq 'basketball'),"CGI::url_param()");
} else {
  print "ok 23 # Skip\n";
  print "ok 24 # Skip\n";
}
test(25,redirect('http://somewhere.else') eq "Status: 302 Moved${CRLF}location: http://somewhere.else${CRLF}${CRLF}","CGI::redirect() 1");
my $h = redirect(-Location=>'http://somewhere.else',-Type=>'text/html');
test(26,$h eq "Status: 302 Moved${CRLF}location: http://somewhere.else${CRLF}Content-Type: text/html; charset=ISO-8859-1${CRLF}${CRLF}","CGI::redirect() 2");
test(27,redirect(-Location=>'http://somewhere.else/bin/foo&bar',-Type=>'text/html') eq "Status: 302 Moved${CRLF}location: http://somewhere.else/bin/foo&bar${CRLF}Content-Type: text/html; charset=ISO-8859-1${CRLF}${CRLF}","CGI::redirect() 2");
