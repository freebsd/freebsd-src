#!./perl

# Test ability to retrieve HTTP request info
######################### We start with some black magic to print on failure.

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib' if -d '../lib';
}

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

# Set up a CGI environment
$ENV{REQUEST_METHOD}='GET';
$ENV{QUERY_STRING}  ='game=chess&game=checkers&weather=dull';
$ENV{PATH_INFO}     ='/somewhere/else';
$ENV{PATH_TRANSLATED} ='/usr/local/somewhere/else';
$ENV{SCRIPT_NAME}   ='/cgi-bin/foo.cgi';
$ENV{SERVER_PROTOCOL} = 'HTTP/1.0';
$ENV{SERVER_PORT} = 8080;
$ENV{SERVER_NAME} = 'the.good.ship.lollypop.com';

test(2,start_form(-action=>'foobar',-method=>GET) eq 
     qq(<FORM METHOD="GET" ACTION="foobar" ENCTYPE="application/x-www-form-urlencoded">\n),
     "start_form()");

test(3,submit() eq qq(<INPUT TYPE="submit" NAME=".submit">),"submit()");
test(4,submit(-name=>'foo',-value=>'bar') eq qq(<INPUT TYPE="submit" NAME="foo" VALUE="bar">),"submit(-name,-value)");
test(5,submit({-name=>'foo',-value=>'bar'}) eq qq(<INPUT TYPE="submit" NAME="foo" VALUE="bar">),"submit({-name,-value})");
test(6,textfield(-name=>'weather') eq qq(<INPUT TYPE="text" NAME="weather" VALUE="dull">),"textfield({-name})");
test(7,textfield(-name=>'weather',-value=>'nice') eq qq(<INPUT TYPE="text" NAME="weather" VALUE="dull">),"textfield({-name,-value})");
test(8,textfield(-name=>'weather',-value=>'nice',-override=>1) eq qq(<INPUT TYPE="text" NAME="weather" VALUE="nice">),
     "textfield({-name,-value,-override})");
test(9,checkbox(-name=>'weather',-value=>'nice') eq qq(<INPUT TYPE="checkbox" NAME="weather" VALUE="nice">weather\n),
     "checkbox()");
test(10,checkbox(-name=>'weather',-value=>'nice',-label=>'forecast') eq 
     qq(<INPUT TYPE="checkbox" NAME="weather" VALUE="nice">forecast\n),
     "checkbox()");
test(11,checkbox(-name=>'weather',-value=>'nice',-label=>'forecast',-checked=>1,-override=>1) eq 
     qq(<INPUT TYPE="checkbox" NAME="weather" VALUE="nice" CHECKED>forecast\n),
     "checkbox()");
test(12,checkbox(-name=>'weather',-value=>'dull',-label=>'forecast') eq 
     qq(<INPUT TYPE="checkbox" NAME="weather" VALUE="dull" CHECKED>forecast\n),
     "checkbox()");

test(13,radio_group(-name=>'game') eq 
     qq(<INPUT TYPE="radio" NAME="game" VALUE="chess" CHECKED>chess <INPUT TYPE="radio" NAME="game" VALUE="checkers">checkers),
     'radio_group()');
test(14,radio_group(-name=>'game',-labels=>{'chess'=>'ping pong'}) eq 
     qq(<INPUT TYPE="radio" NAME="game" VALUE="chess" CHECKED>ping pong <INPUT TYPE="radio" NAME="game" VALUE="checkers">checkers),
     'radio_group()');

test(15, checkbox_group(-name=>'game',-Values=>[qw/checkers chess cribbage/]) eq 
     qq(<INPUT TYPE="checkbox" NAME="game" VALUE="checkers" CHECKED>checkers <INPUT TYPE="checkbox" NAME="game" VALUE="chess" CHECKED>chess <INPUT TYPE="checkbox" NAME="game" VALUE="cribbage">cribbage),
     'checkbox_group()');

test(16, checkbox_group(-name=>'game',-Values=>[qw/checkers chess cribbage/],-Defaults=>['cribbage'],-override=>1) eq 
     qq(<INPUT TYPE="checkbox" NAME="game" VALUE="checkers">checkers <INPUT TYPE="checkbox" NAME="game" VALUE="chess">chess <INPUT TYPE="checkbox" NAME="game" VALUE="cribbage" CHECKED>cribbage),
     'checkbox_group()');

test(17, popup_menu(-name=>'game',-Values=>[qw/checkers chess cribbage/],-Default=>'cribbage',-override=>1) eq <<END,'checkbox_group()');
<SELECT NAME="game">
<OPTION  VALUE="checkers">checkers
<OPTION  VALUE="chess">chess
<OPTION SELECTED VALUE="cribbage">cribbage
</SELECT>
END

