;# $Header: /pub/FreeBSD/FreeBSD-CVS/src/gnu/usr.bin/perl/lib/importenv.pl,v 1.1.1.1 1994/09/10 06:27:52 gclarkii Exp $

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
