#!./perl

BEGIN {
    chdir 't' if -d 't';
    @INC = '../lib';
}

use Tie::Array;
tie @x,Tie::StdArray;
require "../t/op/push.t"
