#From stolfi@ic.unicamp.br  Sun Jan 28 19:02:09 2001
#Received: from mail.actcom.co.il [192.114.47.13]
#	by localhost with POP3 (fetchmail-5.5.0)
#	for arnold@localhost (single-drop); Sun, 28 Jan 2001 19:02:09 +0200 (IST)
#Received: by actcom.co.il (mbox arobbins)
# (with Cubic Circle's cucipop (v1.31 1998/05/13) Sun Jan 28 19:03:34 2001)
#X-From_: stolfi@ic.unicamp.br Sun Jan 28 18:46:02 2001
#Received: from lmail.actcom.co.il by actcom.co.il  with ESMTP
#	(8.9.1a/actcom-0.2) id SAA22932 for <arobbins@actcom.co.il>;
#	Sun, 28 Jan 2001 18:46:00 +0200 (EET)  
#	(rfc931-sender: lmail.actcom.co.il [192.114.47.13])
#Received: from billohost.com (www.billohost.com [209.196.35.10])
#	by lmail.actcom.co.il (8.9.3/8.9.1) with ESMTP id SAA18523
#	for <arobbins@actcom.co.il>; Sun, 28 Jan 2001 18:46:35 +0200
#Received: from grande.dcc.unicamp.br (grande.dcc.unicamp.br [143.106.7.8])
#	by billohost.com (8.9.3/8.9.3) with ESMTP id LAA20063
#	for <arnold@skeeve.com>; Sun, 28 Jan 2001 11:45:54 -0500
#Received: from amazonas.dcc.unicamp.br (amazonas.dcc.unicamp.br [143.106.7.11])
#	by grande.dcc.unicamp.br (8.9.3/8.9.3) with ESMTP id OAA29726;
#	Sun, 28 Jan 2001 14:45:47 -0200 (EDT)
#Received: from coruja.dcc.unicamp.br (coruja.dcc.unicamp.br [143.106.24.80])
#	by amazonas.dcc.unicamp.br (8.8.5/8.8.5) with ESMTP id OAA06542;
#	Sun, 28 Jan 2001 14:45:45 -0200 (EDT)
#Received: (from stolfi@localhost)
#	by coruja.dcc.unicamp.br (8.11.0/8.11.0) id f0SGjib16703;
#	Sun, 28 Jan 2001 14:45:44 -0200 (EDT)
#Date: Sun, 28 Jan 2001 14:45:44 -0200 (EDT)
#Message-Id: <200101281645.f0SGjib16703@coruja.dcc.unicamp.br>
#From: Jorge Stolfi <stolfi@ic.unicamp.br>
#To: Michal Jaegermann <michal@ellpspace.math.ualberta.ca>
#Cc: Aharon Robbins <arnold@skeeve.com>, oliva@ic.unicamp.br,
#        celio@ic.unicamp.br, ducatte@ic.unicamp.br, machado@ic.unicamp.br
#Subject: Re: a regex.c problem
#MIME-Version: 1.0
#Content-Transfer-Encoding: 8bit
#Content-Type: text/plain; charset=iso-8859-1
#In-Reply-To: <20010128090314.A5820@ellpspace.math.ualberta.ca>
#References: <200101281207.f0SC7Un08435@skeeve.com>
#	<20010128090314.A5820@ellpspace.math.ualberta.ca>
#Reply-To: stolfi@ic.unicamp.br
#Status: RO
#
#
#    > [Michal] Are there any other examples of "certain characters"
#    > which would throw this regex engine off?
#
#I now tested [anX]*n for X ranging trough all characters from \000 and
#\377, and got that unexpected result only for the following ones:
#
#  \370 | =F8 | ø | Small o, slash
#  \371 | =F9 | ù | Small u, grave accent
#  \372 | =FA | ú | Small u, acute accent
#  \373 | =FB | û | Small u, circumflex accent
#  \374 | =FC | ü | Small u, dieresis or umlaut mark
#  \375 | =FD | ý | Small y, acute accent
#  \376 | =FE | þ | Small thorn, Icelandic
#  \377 | =FF | ÿ | Small y, dieresis or umlaut mark 
#
#I have also tried those offending REs from inside emacs (20.7.1), with
#query-replace-regexp, and it seems to be working fine. So presumably
#the bug lies in gawk itself, or in the RE parsing code, rather than in
#the matching engine?
#
#Could it be an underdimensioned table somewhere?
#
#Thanks for the help, and all the best
#
#--stolfi
#
#  ----------------------------------------------------------------------
  #! /usr/bin/gawk -f

  BEGIN {
    for (c = 0; c < 256; c++) 
      { do_test(c); }
  }

  function do_test(char,   pat,s,t)
  {
    if (char == 92) { printf "(error for \\%03o)\n", char; return; }
    pat = sprintf("[an\\%03o]*n", char);
    s = "bananas and ananases in canaan";
    t = s; gsub(pat, "AN", t);        printf "%-8s  %s\n", pat, t;
# ADR: Added:
    if (s ~ pat) printf "\tmatch\n" ; else printf "\tno-match\n"
  }

#  ----------------------------------------------------------------------
