# From Servatius.Brandt@fujitsu-siemens.com  Fri Dec  1 13:44:48 2000
# Received: from mail.actcom.co.il
# 	by localhost with POP3 (fetchmail-5.1.0)
# 	for arnold@localhost (single-drop); Fri, 01 Dec 2000 13:44:48 +0200 (IST)
# Received: by actcom.co.il (mbox arobbins)
#  (with Cubic Circle's cucipop (v1.31 1998/05/13) Fri Dec  1 13:44:10 2000)
# X-From_: Servatius.Brandt@fujitsu-siemens.com Fri Dec  1 13:11:23 2000
# Received: from lmail.actcom.co.il by actcom.co.il  with ESMTP
# 	(8.9.1a/actcom-0.2) id NAA11033 for <arobbins@actcom.co.il>;
# 	Fri, 1 Dec 2000 13:11:21 +0200 (EET)  
# 	(rfc931-sender: lmail.actcom.co.il [192.114.47.13])
# Received: from billohost.com (10-209.196.35.dellhost.com [209.196.35.10] (may be forged))
# 	by lmail.actcom.co.il (8.9.3/8.9.1) with ESMTP id NAA30286
# 	for <arobbins@actcom.co.il>; Fri, 1 Dec 2000 13:12:25 +0200
# Received: from fencepost.gnu.org (we-refuse-to-spy-on-our-users@fencepost.gnu.org [199.232.76.164])
# 	by billohost.com (8.9.3/8.9.3) with ESMTP id GAA26074
# 	for <arnold@skeeve.com>; Fri, 1 Dec 2000 06:09:08 -0500
# Received: from energy.pdb.sbs.de ([192.109.2.19])
# 	by fencepost.gnu.org with esmtp (Exim 3.16 #1 (Debian))
# 	id 141o5z-0000RJ-00; Fri, 01 Dec 2000 06:11:16 -0500
# Received: from trulli.pdb.fsc.net ([172.25.96.20])
# 	by energy.pdb.sbs.de (8.9.3/8.9.3) with ESMTP id MAA32687;
# 	Fri, 1 Dec 2000 12:11:13 +0100
# Received: from pdbrd02e.pdb.fsc.net (pdbrd02e.pdb.fsc.net [172.25.96.15])
# 	by trulli.pdb.fsc.net (8.9.3/8.9.3) with ESMTP id MAA27384;
# 	Fri, 1 Dec 2000 12:11:13 +0100
# Received: from Fujitsu-Siemens.com (pgtd1181.mch.fsc.net [172.25.126.152]) by pdbrd02e.pdb.fsc.net with SMTP (Microsoft Exchange Internet Mail Service Version 5.5.2650.21)
# 	id XC2QLXS2; Fri, 1 Dec 2000 12:11:13 +0100
# Message-ID: <3A2786CF.1000903@Fujitsu-Siemens.com>
# Date: Fri, 01 Dec 2000 12:09:03 +0100
# From: Servatius Brandt <Servatius.Brandt@fujitsu-siemens.com>
# Organization: Fujitsu Siemens Computers
# User-Agent: Mozilla/5.0 (Windows; U; Win95; en-US; m18) Gecko/20001108 Netscape6/6.0
# X-Accept-Language: de, en
# MIME-Version: 1.0
# To: bug-gnu-utils@gnu.org
# CC: arnold@gnu.org
# Subject: Bug Report: \y, \B, \<, \> do not work with _
# Content-Type: text/plain; charset=us-ascii; format=flowed
# Content-Transfer-Encoding: 7bit
# Status: R
# 
# Hello,
# 
# The \y, \B, \<, \> patterns do not regard _ as
# word-constituent (unlike \w and \W, which do).
# 
# Operating system: ReliantUNIX-Y 5.44 C2001 RM600 R10000
# Version of gawk: 3.0.6
# C-Compiler: Fujitsu Siemens Computers CDS++ V2.0C0004
# 
# Test program:
# 
#!/usr/local/bin/gawk -f

BEGIN {
     print match("X _abc Y", /\<_abc/)		# bug
     print match("X _abc Y", /\y_abc/)		# bug
     print match("X abc_ Y", /abc_\>/)		# bug
     print match("X abc_ Y", /abc_\y/)		# bug
     print match("X abc_def Y", /abc_\Bdef/)	# bug

     print match("X a_c Y", /a\wc/)		# ok!
     print match("X a.c Y", /a\Wc/)		# ok!
     exit
}
# 
# 
# Regards,
# Servatius Brandt
# 
# 
