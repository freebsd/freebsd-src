#!/usr/local/bin/perl -w

BEGIN {
    chdir('t') if -d 't';
    @INC = '../lib';
}

# Test ability to retrieve HTTP request info
######################### We start with some black magic to print on failure.
use lib '../blib/lib','../blib/arch';

BEGIN {$| = 1; print "1..17\n"; }
END {print "not ok 1\n" unless $loaded;}
use CGI (':standard','-no_debug');
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
if ($^O eq 'VMS') { 
    $CRLF = "\n";  # via web server carriage is inserted automatically
}
if (ord("\t") != 9) { # EBCDIC?
    $CRLF = "\r\n";
}


# Set up a CGI environment
$ENV{REQUEST_METHOD}='GET';
$ENV{QUERY_STRING}  ='game=chess&game=checkers&weather=dull';
$ENV{PATH_INFO}     ='/somewhere/else';
$ENV{PATH_TRANSLATED} ='/usr/local/somewhere/else';
$ENV{SCRIPT_NAME}   ='/cgi-bin/foo.cgi';
$ENV{SERVER_PROTOCOL} = 'HTTP/1.0';
$ENV{SERVER_PORT} = 8080;
$ENV{SERVER_NAME} = 'the.good.ship.lollypop.com';

test(2,start_form(-action=>'foobar',-method=>'get') eq 
     qq(<form method="get" action="foobar" enctype="application/x-www-form-urlencoded">\n),
     "start_form()");

test(3,submit() eq qq(<input type="submit" name=".submit" />),"submit()");
test(4,submit(-name=>'foo',-value=>'bar') eq qq(<input type="submit" name="foo" value="bar" />),"submit(-name,-value)");
test(5,submit({-name=>'foo',-value=>'bar'}) eq qq(<input type="submit" name="foo" value="bar" />),"submit({-name,-value})");
test(6,textfield(-name=>'weather') eq qq(<input type="text" name="weather" value="dull" />),"textfield({-name})");
test(7,textfield(-name=>'weather',-value=>'nice') eq qq(<input type="text" name="weather" value="dull" />),"textfield({-name,-value})");
test(8,textfield(-name=>'weather',-value=>'nice',-override=>1) eq qq(<input type="text" name="weather" value="nice" />),
     "textfield({-name,-value,-override})");
test(9,checkbox(-name=>'weather',-value=>'nice') eq qq(<input type="checkbox" name="weather" value="nice" />weather),
     "checkbox()");
test(10,checkbox(-name=>'weather',-value=>'nice',-label=>'forecast') eq 
     qq(<input type="checkbox" name="weather" value="nice" />forecast),
     "checkbox()");
test(11,checkbox(-name=>'weather',-value=>'nice',-label=>'forecast',-checked=>1,-override=>1) eq 
     qq(<input type="checkbox" name="weather" value="nice" checked />forecast),
     "checkbox()");
test(12,checkbox(-name=>'weather',-value=>'dull',-label=>'forecast') eq 
     qq(<input type="checkbox" name="weather" value="dull" checked />forecast),
     "checkbox()");

test(13,radio_group(-name=>'game') eq 
     qq(<input type="radio" name="game" value="chess" checked />chess <input type="radio" name="game" value="checkers" />checkers),
     'radio_group()');
test(14,radio_group(-name=>'game',-labels=>{'chess'=>'ping pong'}) eq 
     qq(<input type="radio" name="game" value="chess" checked />ping pong <input type="radio" name="game" value="checkers" />checkers),
     'radio_group()');

test(15, checkbox_group(-name=>'game',-Values=>[qw/checkers chess cribbage/]) eq 
     qq(<input type="checkbox" name="game" value="checkers" checked />checkers <input type="checkbox" name="game" value="chess" checked />chess <input type="checkbox" name="game" value="cribbage" />cribbage),
     'checkbox_group()');

test(16, checkbox_group(-name=>'game',-values=>[qw/checkers chess cribbage/],-defaults=>['cribbage'],-override=>1) eq 
     qq(<input type="checkbox" name="game" value="checkers" />checkers <input type="checkbox" name="game" value="chess" />chess <input type="checkbox" name="game" value="cribbage" checked />cribbage),
     'checkbox_group()');
test(17, popup_menu(-name=>'game',-values=>[qw/checkers chess cribbage/],-default=>'cribbage',-override=>1) eq <<END,'checkbox_group()');
<select name="game">
<option  value="checkers">checkers</option>
<option  value="chess">chess</option>
<option selected value="cribbage">cribbage</option>
</select>
END

