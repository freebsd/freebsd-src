#To: bug-gnu-utils@gnu.org
#From: Kristján Jónasson <kristjan@decode.is>
#Subject: Gawk bug
#Cc: arnold@gnu.org
#
#Hi!
#
#The following seems to be a bug in gawk. I have tried as I could to
#minimize the bug-causing program, so of course it does not seem to do
#anything useful in its present form. The error message received is:
#
#gawk: test.awk:15: fatal error: internal error
#Aborted
#
#Note that there is an attached file that the program reads, called "a". I
#played with the program a fair bit and my feeling is that the error is
#related with the delete statement, and not the reading of the file and the
#close statement. At one point I was able to remove the file reading and
#still obtain the error. If, for example, I remove the close statement and
#make two copies of the file instead, (reading one copy in sub1 and the
#other in sub2), the error still occurs.
#
#The operating system is Red Hat Linux, version 6.0, the gawk is version
#3.0.4, and the gawk was obtained from an rpm file gawk-3.0.4-1.i386.rpm.
#
#The program is:
#

# Wed Mar  8 13:41:34 IST 2000
# ADR: modified to use INPUT, so can set it from command line.
#      When run, no output is produced, but it shouldn't core
#      dump, either.
#
# The program bug is to not close the file in sub2.

function sub1(x) {
# while (getline < "a" == 1) i++
  while (getline < INPUT == 1) i++
# close("a")
  close(INPUT)
}

function sub2(x) {
  i=0
  delete y
# while (getline < "a" == 1) z[++i] = $1
  while (getline < INPUT == 1) z[++i] = $1
  for(i in z) y[i] = x[i] + z[i]
}

function sub3(x, y, z) {
  sub2(x)
  for(i=1; i<=4; i++) z[i] = y[i]
}

BEGIN {
  sub1(x)
  sub2(x)
  sub3(x, y, z)
}
#
#And the data file is:
#
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
# 32.440    3.830    3.383700000000000    10.08    298  865
#
#
