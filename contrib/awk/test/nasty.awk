#From hankedr@manatee.dms.auburn.edu  Tue Oct 13 22:15:59 1998
#Return-Path: <hankedr@manatee.dms.auburn.edu>
#Received: from cssun.mathcs.emory.edu (cssun.mathcs.emory.edu [170.140.150.1])
#	by dmx.netvision.net.il (8.9.0.Beta5/8.8.6) with ESMTP id PAA03924
#	for <arobbins@netvision.net.il>; Tue, 13 Oct 1998 15:32:13 +0200 (IST)
#Received: from mescaline.gnu.org (we-refuse-to-spy-on-our-users@mescaline.gnu.org [158.121.106.21]) by cssun.mathcs.emory.edu (8.7.5/8.6.9-940818.01cssun) with ESMTP id KAA11644 for <arnold@mathcs.emory.edu>; Tue, 13 Oct 1998 10:22:32 -0400 (EDT)
#Received: from manatee.dms.auburn.edu (manatee.dms.auburn.edu [131.204.53.104])
#	by mescaline.gnu.org (8.9.1a/8.9.1) with ESMTP id KAA03250
#	for <arnold@gnu.org>; Tue, 13 Oct 1998 10:25:32 -0400
#Received: (from hankedr@localhost)
#	by manatee.dms.auburn.edu (8.9.1a/8.9.1) id JAA13348;
#	Tue, 13 Oct 1998 09:22:29 -0500 (CDT)
#Date: Tue, 13 Oct 1998 09:22:29 -0500 (CDT)
#Message-Id: <199810131422.JAA13348@manatee.dms.auburn.edu>
#From: Darrel Hankerson <hankedr@dms.auburn.edu>
#To: arnold@gnu.org
#In-reply-to: <199810131313.QAA31784@alpha.netvision.net.il> (message from
#	Aharon Robbins on Tue, 13 Oct 1998 16:10:36 +0200)
#Subject: Re: full text of bug report?
#Mime-Version: 1.0
#Content-Type: text/plain; charset=US-ASCII
#X-UIDL: bf3fce492dad4ab030c561e7b2f27d0a
#Status: RO
#
#   Do you have the full text of the	a = a "\n" f() 	bug report?
#   I can't find it.... I'm not sure there really is a bug.
#
#Yes, see below.  
#
#His example has unnecessary fragments (in particular, the use of
#gensub is irrelevant).  As I wrote to you earlier, the interesting
#question for me is:
#
#   Is the concatenation result undefined?  If the result is defined or
#   implementation-dependent, then gawk has a bug.
#
#
#=== Original report =====================================================
#From: Attila Torcsvari <arcdev@mail.matav.hu>
#To: "'bug-gnu-utils@prep.ai.mit.edu'" <bug-gnu-utils@gnu.org>
#Subject: gawk 3.0.3 bug
#Date: Thu, 17 Sep 1998 18:12:13 +0200
#MIME-Version: 1.0
#Content-Transfer-Encoding: 7bit
#Resent-From: bug-gnu-utils@gnu.org
#X-Mailing-List: <bug-gnu-utils@gnu.org> archive/latest/3396
#X-Loop: bug-gnu-utils@gnu.org
#Precedence: list
#Resent-Sender: bug-gnu-utils-request@gnu.org
#Content-Transfer-Encoding: 7bit
#Content-Type: text/plain; charset="us-ascii"
#Content-Length: 618
#
#Bug-gnuers,
#please pass it to the responsible.
#
#The following generates something interesting:
#
BEGIN{
a="aaaaa"
a=a a #10
a=a a #20
a=a a #40
a=a a #80
a=a a #160
a=a a # i.e. a is long enough

a=a"\n"f() # this causes the trouble
print a # guess the result
}

function f()
{
#print "a before: ", a
#a=gensub("a","123,","g",a) # 'a' will be just a bit longer (4 times, but still should fit: 4*160=640)
gsub(/a/, "123", a)
#print "a after: ", a
return "X"
}
#
#Possible reason:
#during f the a is modified,
#it can be even freed, because gensub modifies its size
#the printout contains trash.
#
#Used version: VC compiled WinNT 32 bit Intel.
#
#Regards,
#
#Attila Torcsvari
#Arcanum Development
#
