# Fall back to -O optimization to avoid known gcc 2.8.0 -O2 problems on MPE/iX.
#  Mark Bixby <markb@cccd.edu>
$self->{OPTIMIZE} = '-O';
