#!./perl

BEGIN {
    chdir 't' if -d 't';
    unshift @INC, '../lib';
}

use Tie::Array;
tie @foo,Tie::StdArray;
tie @ary,Tie::StdArray;
tie @bar,Tie::StdArray;
require "op/array.t"
