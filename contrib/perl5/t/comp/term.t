#!./perl

# $RCSfile: term.t,v $$Revision: 4.1 $$Date: 92/08/07 18:27:24 $

# tests that aren't important enough for base.term

print "1..22\n";

$x = "\\n";
print "#1\t:$x: eq " . ':\n:' . "\n";
if ($x eq '\n') {print "ok 1\n";} else {print "not ok 1\n";}

$x = "#2\t:$x: eq :\\n:\n";
print $x;
unless (index($x,'\\\\')>0) {print "ok 2\n";} else {print "not ok 2\n";}

if (length('\\\\') == 2) {print "ok 3\n";} else {print "not ok 3\n";}

$one = 'a';

if (length("\\n") == 2) {print "ok 4\n";} else {print "not ok 4\n";}
if (length("\\\n") == 2) {print "ok 5\n";} else {print "not ok 5\n";}
if (length("$one\\n") == 3) {print "ok 6\n";} else {print "not ok 6\n";}
if (length("$one\\\n") == 3) {print "ok 7\n";} else {print "not ok 7\n";}
if (length("\\n$one") == 3) {print "ok 8\n";} else {print "not ok 8\n";}
if (length("\\\n$one") == 3) {print "ok 9\n";} else {print "not ok 9\n";}
if (length("\\${one}") == 2) {print "ok 10\n";} else {print "not ok 10\n";}

if ("${one}b" eq "ab") { print "ok 11\n";} else {print "not ok 11\n";}

@foo = (1,2,3);
if ("$foo[1]b" eq "2b") { print "ok 12\n";} else {print "not ok 12\n";}
if ("@foo[0..1]b" eq "1 2b") { print "ok 13\n";} else {print "not ok 13\n";}
$" = '::';
if ("@foo[0..1]b" eq "1::2b") { print "ok 14\n";} else {print "not ok 14\n";}

# test if C<eval "{...}"> distinguishes between blocks and hashrefs

$a = "{ '\\'' , 'foo' }";
$a = eval $a;
if (ref($a) eq 'HASH') {print "ok 15\n";} else {print "not ok 15\n";}

$a = "{ '\\\\\\'abc' => 'foo' }";
$a = eval $a;
if (ref($a) eq 'HASH') {print "ok 16\n";} else {print "not ok 16\n";}

$a = "{'a\\\n\\'b','foo'}";
$a = eval $a;
if (ref($a) eq 'HASH') {print "ok 17\n";} else {print "not ok 17\n";}

$a = "{'\\\\\\'\\\\'=>'foo'}";
$a = eval $a;
if (ref($a) eq 'HASH') {print "ok 18\n";} else {print "not ok 18\n";}

$a = "{q,a'b,,'foo'}";
$a = eval $a;
if (ref($a) eq 'HASH') {print "ok 19\n";} else {print "not ok 19\n";}

$a = "{q[[']]=>'foo'}";
$a = eval $a;
if (ref($a) eq 'HASH') {print "ok 20\n";} else {print "not ok 20\n";}

# needs disambiguation if first term is a variable
$a = "+{ \$a , 'foo'}";
$a = eval $a;
if (ref($a) eq 'HASH') {print "ok 21\n";} else {print "not ok 21\n";}

$a = "+{ \$a=>'foo'}";
$a = eval $a;
if (ref($a) eq 'HASH') {print "ok 22\n";} else {print "not ok 22\n";}
