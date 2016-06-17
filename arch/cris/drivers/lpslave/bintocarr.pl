#!/usr/bin/perl -w

# Copy of mkjulbin.pl made by Olof
# convert a binary stdin to a C-file containing a char array of the input
# first argument is the symbol name

$symbol = shift @ARGV;

print "#include <linux/init.h>\n\n";
#print "unsigned char $symbol", "[] __initdata = {\n";
print "unsigned char $symbol", "[]  = {\n";

my $char;

$bcount = 0;

while(read STDIN, $char, 1) {
    printf("0x%x, ", ord($char));
    $bcount++;
    if(!($bcount % 16)) {
	print "\n";
    }
}

print "\n};\n";

$lensymb = ("_" . ($symbol . "_length"));

print "__asm__(\"\\t.globl $lensymb\\n$lensymb = $bcount\\n\");\n";
