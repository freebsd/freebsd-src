#From dhw@gamgee.acad.emich.edu  Sat Oct 31 22:54:07 1998
#Return-Path: <dhw@gamgee.acad.emich.edu>
#Received: from cssun.mathcs.emory.edu (cssun.mathcs.emory.edu [170.140.150.1])
#	by amx.netvision.net.il (8.9.0.Beta5/8.8.6) with ESMTP id HAA08891
#	for <arobbins@netvision.net.il>; Sat, 31 Oct 1998 07:14:07 +0200 (IST)
#Received: from mescaline.gnu.org (we-refuse-to-spy-on-our-users@mescaline.gnu.org [158.121.106.21]) by cssun.mathcs.emory.edu (8.7.5/8.6.9-940818.01cssun) with ESMTP id AAA14947 for <arnold@mathcs.emory.edu>; Sat, 31 Oct 1998 00:14:32 -0500 (EST)
#Received: from gamgee.acad.emich.edu (gamgee.acad.emich.edu [164.76.102.76])
#	by mescaline.gnu.org (8.9.1a/8.9.1) with SMTP id AAA20645
#	for <arnold@gnu.ai.mit.edu>; Sat, 31 Oct 1998 00:17:54 -0500
#Received: by gamgee.acad.emich.edu (Smail3.1.29.1 #57)
#	id m0zZUKY-000IDSC; Sat, 31 Oct 98 00:16 CST
#Message-Id: <m0zZUKY-000IDSC@gamgee.acad.emich.edu>
#Date: Sat, 31 Oct 98 00:16 CST
#From: dhw@gamgee.acad.emich.edu (David H. West)
#To: bug-gnu-utils@gnu.org
#Subject: gawk 3.0.3 bug report
#Cc: arnold@gnu.org
#X-UIDL: 7474b825cff989adf38f13883d84fdd7
#Status: RO
#
#gawk version: 3.03
#System used: Linux, kernel 2.0.28, libc 5.4.33, AMD K5PR133 (i586 clone)
#Remark: There seems to be at least one bug shown by the demo below. 
#        There may also be a Dark Corner involving the value of NR in an
#        END block, a topic on which the info file is silent.  In gawk
#        3.0.3, NR often seems to have the least-surprise value in an
#        END block, but sometimes it doesn't - see example below.
#Problem descr: the log below shows a case where:
#        a) (this may be a red herring) the output of the gawk script
#           is different depending on whether its input file is named on
#           the command line or catted to stdin, without any use of the
#           legitimate means which could produce this effect.
#        b) NR is clearly getting clobbered; I have tried to simplify
#           the 19-line script "awkerr1" below, but seemingly unrelated
#           changes, like shortening constant strings which appear only in
#           print statements, or removing unexecuted or irrelevant code,
#           cause the clobbering to go away.  Some previous (larger)
#           versions of this code would clobber NR also when reading from
#           stdin, but I thought you'd prefer a shorter example :-).
#Reproduce-By: using the gawk script "awkerr1", the contents of
#              which appear in the transcript below as the output of the
#              command "cat awkerr1".  Comments following # were added
#              to the transcript later as explanation.
#---------------------------------------------- Script started on Fri
#Oct 30 20:04:16 1998 chipmunk:/ram0# ls -l a1 awkerr1 -rw-r--r--   1
#root     root            2 Oct 30 18:42 a1 -rwxr-xr-x   1 root     root
#389 Oct 30 19:54 awkerr1 chipmunk:/ram0# cat a1            #a1 contains
#one printable char and a newline a chipmunk:/ram0# od -c xc a1
#0000000 0a61
#          a  \n
#0000002 chipmunk:/ram0# cat a1 | awkerr1           #no surprises here
#1 lines in 1 sec: 1 lines/sec;  nlines=1 chipmunk:/ram0# awkerr1 a1 È
#lines in 1 sec: 1 lines/sec;  nlines=1    #?! first char is an uppercase
#E-grave chipmunk:/ram0# awkerr1 a1 | od -N1 -xc 0000000 00c8
#        310  \0
#0000001 chipmunk:/ram0# cat awkerr1   #the apparent ^M's are not
#actually in the file
#!/usr/bin/awk -f
function process(w) {
   if(w in ws) {
      printf " : found\n"; lc[p " " w]++; rc[w " " n]++; }
   }
BEGIN {IGNORECASE=1;
      }
/^/ {if(NR % 10 ==0)print "processing line " NR;
     process($1); nlines++;
    }
END {p=w; w=n; n="";
     if(w)process(w); t=1; print NR " lines in " t " sec: " NR+0 " lines/sec;  nlines=" nlines;
    }
#chipmunk:/ram0# exit Script done on Fri Oct 30 20:07:31 1998
#---------------------------------------------
#
#-David West     dhw@gamgee.acad.emich.edu
#
