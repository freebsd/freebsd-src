#!/usr/bin/perl
#
# makes perl scripts smaller, useful for boot floppies 

while(<>) {
    s/(\s|^|;)#.*//; 			# comments
    s/\s+$//; 				# leading spaces
    s/^\s+//; 				# ending spaces
    s/\s+/ /g; 				# double spaces
    s/\s*([=\(\{\)\}])\s*/$1/g;		# spaces around =(){}
    print "$_\n" if $_;
}
