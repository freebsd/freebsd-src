
			Secure DNS (TIS/DNSSEC)
			    September 1996

Copyright (C) 1995,1996 Trusted Information Systems, Incorporated

Trusted Information Systems, Inc. has received approval from the
United States Government for export and reexport of TIS/DNSSEC
software from the United States of America under the provisions of
the Export Administration Regulations (EAR) General Software Note
(GSN) license exception for mass market software.  Under the
provisions of this license, this software may be exported or
reexported to all destinations except for the embargoed countries of
Cuba, Iran, Iraq, Libya, North Korea, Sudan and Syria.  Any export
or reexport of TIS/DNSSEC software to the embargoed countries
requires additional, specific licensing approval from the United
States Government.

Trusted Information Systems, Inc., is pleased to
provide a reference implementation of the secure Domain Name System
(TIS/DNSSEC).  In order to foster acceptance of secure DNS and provide
the community with a usable, working version of this technology,
TIS/DNSSEC is being made available for broad use on the following basis.

- Trusted Information Systems makes no representation about the
  suitability of this software for any purpose.  It is provided "as is"
  without express or implied warranty.

- TIS/DNSSEC is distributed in source code form, with all modules written
  in the C programming language.  It runs on many UNIX derived platforms
  and is integrated with the Bind implementation of the DNS protocol.

- This beta version of TIS/DNSSEC may be used, copied, and modified for
  testing and evaluation purposes without fee during the beta test
  period, provided that this notice appears in supporting documentation
  and is retained in all software modules in which it appears.  Any other
  use requires specific, written prior permission from Trusted Information
  Systems.

TIS maintains the email distribution list dns-security@tis.com for
discussion of secure DNS.  To join, send email to
	dns-security-request@tis.com.

TIS/DNSSEC technical questions and bug reports should be addressed to
	dns-security@tis.com. 

To reach the maintainers of TIS/DNSSEC send mail to
	tisdnssec-support@tis.com

TIS/DNSSEC is a product of Trusted Information Systems, Inc.

This is an beta version of Bind with secure DNS extensions it uses 
RSAREF which you must obtain separately.

Implemented and tested in this version:
	Portable key storage format. 
	Improved authentication API 
	Support for using different authentication packages.
	All Security RRs including KEY SIG, NXT, and support for wild cards
	tool for generating KEYs 
	tool for signing RRs in boot files
	verification of RRs on load 
	verification of RRs over the wire
	transmission of SIG RRs
	returns NXT when name and/or type does not exist
	storage of NXT, KEY, and SIG RRs with CNAME RR
	AD/ID bits added to header and setting of these bits
	key storage and retrieval
	dig and nslookup can display new header bits and RRs
	AXFR signature RR
	keyfile directive 
	$SIGNER directive (to turn on and off signing)
	adding KEY to answers with NS or SOA
	SOA sequence numbers are now set each time zone is signed
	SIG AXFR ignores label count of names
	generation and inclusion of .PARENT files
	Returns only one NXT at delegation points unless two are required
	Expired SIG records are now returned in response to query
	
Implemented but not fully tested:

Known bugs:
	
Not implemented:
	ROUND_ROBIN behaviour 
	zone transfer in SIG(AXFR) sort order. 
	transaction SIGs
	verification in resolver. (stub resolvers must trust local servers
		resolver library is to low level to implement security)
	knowing when to trust the AD bit in responses

Read files INSTALL_SEC and USAGE_SEC for installation and user
instructions, respectively.
