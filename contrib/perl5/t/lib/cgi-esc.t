#!/usr/local/bin/perl -w

BEGIN {
    chdir('t') if -d 't';
    @INC = '../lib';
}

# Test ability to escape() and unescape() punctuation characters
# except for qw(- . _).
######################### We start with some black magic to print on failure.
use lib '../blib/lib','../blib/arch';

BEGIN {$| = 1; print "1..59\n"; }
END {print "not ok 1\n" unless $loaded;}
use Config;
use CGI::Util qw(escape unescape);
$loaded = 1;
print "ok 1\n";

######################### End of black magic.

# util
sub test {
    local($^W) = 0;
    my($num, $true,$msg) = @_;
    print($true ? "ok $num\n" : "not ok $num $msg\n");
}

# ASCII order, ASCII codepoints, ASCII repertoire

my %punct = (
    ' ' => '20',  '!' => '21',  '"' => '22',  '#' =>  '23', 
    '$' => '24',  '%' => '25',  '&' => '26',  '\'' => '27', 
    '(' => '28',  ')' => '29',  '*' => '2A',  '+' =>  '2B', 
    ',' => '2C',                              '/' =>  '2F',  # '-' => '2D',  '.' => '2E' 
    ':' => '3A',  ';' => '3B',  '<' => '3C',  '=' =>  '3D', 
    '>' => '3E',  '?' => '3F',  '[' => '5B',  '\\' => '5C', 
    ']' => '5D',  '^' => '5E',                '`' =>  '60',  # '_' => '5F',
    '{' => '7B',  '|' => '7C',  '}' => '7D',  '~' =>  '7E', 
         );

# The sort order may not be ASCII on EBCDIC machines:

my $i = 1;

foreach(sort(keys(%punct))) { 
    $i++;
    my $escape = "AbC\%$punct{$_}dEF";
    my $cgi_escape = escape("AbC$_" . "dEF");
    test($i, $escape eq $cgi_escape , "# $escape ne $cgi_escape");
    $i++;
    my $unescape = "AbC$_" . "dEF";
    my $cgi_unescape = unescape("AbC\%$punct{$_}dEF");
    test($i, $unescape eq $cgi_unescape , "# $unescape ne $cgi_unescape");
}

