;# $Header: /home/ncvs/src/gnu/usr.bin/perl/lib/importenv.pl,v 1.1.1.1.6.1 1996/06/05 02:42:17 jkh Exp $

;# This file, when interpreted, pulls the environment into normal variables.
;# Usage:
;#	require 'importenv.pl';
;# or
;#	#include <importenv.pl>

local($tmp,$key) = '';

foreach $key (keys(ENV)) {
    $tmp .= "\$$key = \$ENV{'$key'};" if $key =~ /^[A-Za-z]\w*$/;
}
eval $tmp;

1;
