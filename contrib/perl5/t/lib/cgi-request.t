#!/usr/local/bin/perl -w

BEGIN {
    chdir('t') if -d 't';
    @INC = '../lib';
}

# Test ability to retrieve HTTP request info
######################### We start with some black magic to print on failure.
use lib '../blib/lib','../blib/arch';

BEGIN {$| = 1; print "1..33\n"; }
END {print "not ok 1\n" unless $loaded;}
use CGI ();
use Config;
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# util
sub test {
    local($^W) = 0;
    my($num, $true,$msg) = @_;
    print($true ? "ok $num\n" : "not ok $num $msg\n");
}

# Set up a CGI environment
$ENV{REQUEST_METHOD}  = 'GET';
$ENV{QUERY_STRING}    = 'game=chess&game=checkers&weather=dull';
$ENV{PATH_INFO}       = '/somewhere/else';
$ENV{PATH_TRANSLATED} = '/usr/local/somewhere/else';
$ENV{SCRIPT_NAME}     = '/cgi-bin/foo.cgi';
$ENV{SERVER_PROTOCOL} = 'HTTP/1.0';
$ENV{SERVER_PORT}     = 8080;
$ENV{SERVER_NAME}     = 'the.good.ship.lollypop.com';
$ENV{REQUEST_URI}     = "$ENV{SCRIPT_NAME}$ENV{PATH_INFO}?$ENV{QUERY_STRING}";
$ENV{HTTP_LOVE}       = 'true';

$q = new CGI;
test(2,$q,"CGI::new()");
test(3,$q->request_method eq 'GET',"CGI::request_method()");
test(4,$q->query_string eq 'game=chess;game=checkers;weather=dull',"CGI::query_string()");
test(5,$q->param() == 2,"CGI::param()");
test(6,join(' ',sort $q->param()) eq 'game weather',"CGI::param()");
test(7,$q->param('game') eq 'chess',"CGI::param()");
test(8,$q->param('weather') eq 'dull',"CGI::param()");
test(9,join(' ',$q->param('game')) eq 'chess checkers',"CGI::param()");
test(10,$q->param(-name=>'foo',-value=>'bar'),'CGI::param() put');
test(11,$q->param(-name=>'foo') eq 'bar','CGI::param() get');
test(12,$q->query_string eq 'game=chess;game=checkers;weather=dull;foo=bar',"CGI::query_string() redux");
test(13,$q->http('love') eq 'true',"CGI::http()");
test(14,$q->script_name eq '/cgi-bin/foo.cgi',"CGI::script_name()");
test(15,$q->url eq 'http://the.good.ship.lollypop.com:8080/cgi-bin/foo.cgi',"CGI::url()");
test(16,$q->self_url eq 
     'http://the.good.ship.lollypop.com:8080/cgi-bin/foo.cgi/somewhere/else?game=chess;game=checkers;weather=dull;foo=bar',
     "CGI::url()");
test(17,$q->url(-absolute=>1) eq '/cgi-bin/foo.cgi','CGI::url(-absolute=>1)');
test(18,$q->url(-relative=>1) eq 'foo.cgi','CGI::url(-relative=>1)');
test(19,$q->url(-relative=>1,-path=>1) eq 'foo.cgi/somewhere/else','CGI::url(-relative=>1,-path=>1)');
test(20,$q->url(-relative=>1,-path=>1,-query=>1) eq 
     'foo.cgi/somewhere/else?game=chess;game=checkers;weather=dull;foo=bar',
     'CGI::url(-relative=>1,-path=>1,-query=>1)');
$q->delete('foo');
test(21,!$q->param('foo'),'CGI::delete()');

$q->_reset_globals;
$ENV{QUERY_STRING}='mary+had+a+little+lamb';
test(22,$q=new CGI,"CGI::new() redux");
test(23,join(' ',$q->keywords) eq 'mary had a little lamb','CGI::keywords');
test(24,join(' ',$q->param('keywords')) eq 'mary had a little lamb','CGI::keywords');
test(25,$q=new CGI('foo=bar&foo=baz'),"CGI::new() redux");
test(26,$q->param('foo') eq 'bar','CGI::param() redux');
test(27,$q=new CGI({'foo'=>'bar','bar'=>'froz'}),"CGI::new() redux 2");
test(28,$q->param('bar') eq 'froz',"CGI::param() redux 2");

# test tied interface
my $p = $q->Vars;
test(29,$p->{bar} eq 'froz',"tied interface fetch");
$p->{bar} = join("\0",qw(foo bar baz));
test(30,join(' ',$q->param('bar')) eq 'foo bar baz','tied interface store');

# test posting
$q->_reset_globals;
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
  test(31,$q=new CGI,"CGI::new() from POST");
  test(32,$q->param('weather') eq 'nice',"CGI::param() from POST");
  test(33,$q->url_param('big_balls') eq 'basketball',"CGI::url_param()");
} else {
  print "ok 31 # Skip\n";
  print "ok 32 # Skip\n";
  print "ok 33 # Skip\n";
}
