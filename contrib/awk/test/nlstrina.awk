# From E.Ab@chem.rug.nl  Wed Aug  2 13:16:53 2000
# Received: from mail.actcom.co.il
# 	by localhost with POP3 (fetchmail-5.1.2)
# 	for arnold@localhost (single-drop); Wed, 02 Aug 2000 13:16:53 -0400 (EDT)
# Received: from lmail.actcom.co.il by actcom.co.il  with ESMTP
# 	(8.9.1a/actcom-0.2) id MAA21699 for <arobbins@actcom.co.il>;
# 	Wed, 2 Aug 2000 12:20:38 +0300 (EET DST)  
# 	(rfc931-sender: lmail.actcom.co.il [192.114.47.13])
# Received: from freefriends.org (freefriends.org [63.85.55.109])
# 	by lmail.actcom.co.il (8.9.3/8.9.1) with ESMTP id LAA22723
# 	for <arobbins@actcom.co.il>; Wed, 2 Aug 2000 11:23:22 +0300
# Received: from mescaline.gnu.org (mescaline.gnu.org [158.121.106.21])
# 	by freefriends.org (8.9.3/8.9.3) with ESMTP id FAA23582
# 	for <arnold@skeeve.com>; Wed, 2 Aug 2000 05:18:59 -0400
# Received: from dep.chem.rug.nl (dep.chem.rug.nl [129.125.7.81])
# 	by mescaline.gnu.org (8.9.1a/8.9.1) with ESMTP id FAA30670;
# 	Wed, 2 Aug 2000 05:20:24 -0400
# Received: from rugmd34.chem.rug.nl (rugmd34.chem.rug.nl [129.125.42.34])
# 	by dep.chem.rug.nl (8.9.3/8.9.3/Debian 8.9.3-21) with ESMTP id LAA17089;
# 	Wed, 2 Aug 2000 11:20:23 +0200
# Received: from chem.rug.nl (localhost [127.0.0.1]) by rugmd34.chem.rug.nl (980427.SGI.8.8.8/980728.SGI.AUTOCF) via ESMTP id LAA25392; Wed, 2 Aug 2000 11:20:22 +0200 (MDT)
# Sender: E.Ab@chem.rug.nl
# Message-ID: <3987E7D5.2BDC5FD3@chem.rug.nl>
# Date: Wed, 02 Aug 2000 11:20:21 +0200
# From: Eiso AB <E.Ab@chem.rug.nl>
# X-Mailer: Mozilla 4.72C-SGI [en] (X11; I; IRIX 6.5 IP32)
# X-Accept-Language: en
# MIME-Version: 1.0
# To: bug-gnu-utils@gnu.org, arnold@gnu.org
# Subject: bug? [GNU Awk 3.0.5]
#  
# Content-Type: text/plain; charset=us-ascii
# Content-Transfer-Encoding: 7bit
# X-UIDL: \f8"!(8G!!ZL$#!h>X!!
# Status: R
# 
# hi Arnold,
# 
# 
# Please try the script beneath...
# I'm not sure if this is a bug or not, but I would expect 
# the empty string as an array index just to be treated 
# like any other string
# 
# so if ("" in ta) would be true, and for ( i in ta ) should loop only once.
# 
BEGIN {
        v=""
        ta[v]++
        if ( v in ta) print "a",v,++ta[v],ta[v]
	print "b",v,++ta[v],ta[v]
        for( i in ta) print "c",++c,i,ta[i]
} 
# 
# goodluck, Eiso
# 
# -- 
#                                 _________
# _______________________________/ Eiso AB \_________________________
# 
#            o                 
#                                               
#                  o               Dept. of Biochemistry
#                                  University of Groningen		
#                                  The Netherlands                      
#                   o  
#             . .     
#          o   ^                   mailto:eiso@chem.rug.nl
#          |   -   _               mailto:eiso@dds.nl
#           \__|__/                http://md.chem.rug.nl/~eiso
#              |	                 tel 4326
#              |
#             / \
#            /   \
#            |   |
# ________ ._|   |_. ________________________________________________
# 
