#
# Test program from:
#
# Date: Tue, 21 Feb 95 16:09:29 EST
# From: emory!blackhawk.com!aaron (Aaron Sosnick)
# 
BEGIN {
    foo[1]=1;
    foo[2]=2;
    bug1(foo);
}
function bug1(i) {
    for (i in foo) {
	bug2(i);
	delete foo[i];
	print i,1,bot[1];
    }
}
function bug2(arg) {
    bot[arg]=arg;
}
