#!./perl

BEGIN {
    chdir 't' if -d 't';
    unshift @INC, '../lib';
}

use Tie::Array;
tie @x,Tie::StdArray;
require "op/push.t"
