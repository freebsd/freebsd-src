#!/bin/perl -w

$seq = 1;
$file = "";
$file = sprintf("tmp/%02d.m4", $seq);
open(FILE, ">$file") || die "cannot open \"$file\": $!";
printf(STDOUT "FILE: $file\n");
while (<>) {
    if (/^$/) {
	close(FILE);
	$seq++;
	$file = sprintf("tmp/%02d.m4", $seq);
	open(FILE, ">$file") || die "cannot open \"$file\": $!";
	printf(STDOUT "FILE: $file\n");
	next;
    }
    printf FILE;
}
close(FILE);
