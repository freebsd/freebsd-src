;	$Id: named.boot,v 1.3.6.1 1997/05/08 15:25:32 joerg Exp $
;	From: @(#)named.boot	5.1 (Berkeley) 6/30/90

; Refer to the named(8) man page for details.  If you are ever going
; to setup a primary server, make sure you've understood the hairy
; details of how DNS is working.  Even with simple mistakes, you can
; break connectivity for affected parties, or cause huge amount of
; useless Internet traffic.
;
; Setting up secondaries is way easier and the rough picture for this
; is explained below.
;
; If you enable a local name server, don't forget to enter 127.0.0.1
; into your /etc/resolv.conf so this server will be queried first.
; Also, make sure to enable it in /etc/rc.conf.

; example sortlist config:
; sortlist 127.0.0.0

directory	/etc/namedb

; type    domain		source host/file		backup file

cache     .							named.root
primary   0.0.127.IN-ADDR.ARPA	localhost.rev

; NB: Do not use the IP addresses below, they are faked, and only
; serve demonstration/documentation purposes!
;
; Example secondary config entries.  It can be convenient to become
; a secondary at least for the zone where your own domain is in.  Ask           
; your network administrator for the IP address of the responsible              
; primary.                                                                      
;
; Never forget to include the reverse lookup (IN-ADDR.ARPA) zone!
; (This is the first bytes of the respective IP address, in reverse             
; order, with ".IN-ADDR.ARPA" appended.)                                        
;
; Before starting to setup a primary zone, better make sure you fully
; understand how DNS and BIND works, however.  There are sometimes   
; unobvious pitfalls.  Setting up a secondary is comparably simpler.
;
; NB: Don't blindly enable the examples below. :-)  Use actual names
; and addresses instead.                                              
;
;type       zone name               IP of primary  backup file name
;==================================================================     
;secondary  domain.com              192.168.1.1    domain.com.bak  
;secondary  0.168.192.in-addr.arpa  192.168.1.1    0.168.192.in-addr.arpa.bak
;
; 
; If you've got a DNS server around at your upstream provider, enter            
; its IP address here, and enable the line below.  This will make you
; benefit from its cache, thus reduce overall DNS traffic in the Internet.
;
;forwarders 127.0.0.1
;
; In addition to the "forwarders" clause, you can force your name
; server to never initiate queries of its own, but always ask its
; forwarders only, by enabling the following line:
;
;options forward-only
