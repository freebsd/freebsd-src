Network Working Group                                       M. Stahl
Request for Comments: 1032                         SRI International
                                                       November 1987


                      DOMAIN ADMINISTRATORS GUIDE


STATUS OF THIS MEMO

   This memo describes procedures for registering a domain with the
   Network Information Center (NIC) of Defense Data Network (DDN), and
   offers guidelines on the establishment and administration of a domain
   in accordance with the requirements specified in RFC-920.  It is
   intended for use by domain administrators.  This memo should be used
   in conjunction with RFC-920, which is an official policy statement of
   the Internet Activities Board (IAB) and the Defense Advanced Research
   Projects Agency (DARPA).  Distribution of this memo is unlimited.

BACKGROUND

   Domains are administrative entities that provide decentralized
   management of host naming and addressing.  The domain-naming system
   is distributed and hierarchical.

   The NIC is designated by the Defense Communications Agency (DCA) to
   provide registry services for the domain-naming system on the DDN and
   DARPA portions of the Internet.

   As registrar of top-level and second-level domains, as well as
   administrator of the root domain name servers on behalf of DARPA and
   DDN, the NIC is responsible for maintaining the root server zone
   files and their binary equivalents.  In addition, the NIC is
   responsible for administering the top-level domains of "ARPA," "COM,"
   "EDU," "ORG," "GOV," and "MIL" on behalf of DCA and DARPA until it
   becomes feasible for other appropriate organizations to assume those
   responsibilities.

   It is recommended that the guidelines described in this document be
   used by domain administrators in the establishment and control of
   second-level domains.

THE DOMAIN ADMINISTRATOR

   The role of the domain administrator (DA) is that of coordinator,
   manager, and technician.  If his domain is established at the second
   level or lower in the tree, the DA must register by interacting with
   the management of the domain directly above his, making certain that



Stahl                                                           [Page 1]

RFC 1032              DOMAIN ADMINISTRATORS GUIDE          November 1987


   his domain satisfies all the requirements of the administration under
   which his domain would be situated.  To find out who has authority
   over the name space he wishes to join, the DA can ask the NIC
   Hostmaster.  Information on contacts for the top-level and second-
   level domains can also be found on line in the file NETINFO:DOMAIN-
   CONTACTS.TXT, which is available from the NIC via anonymous FTP.

   The DA should be technically competent; he should understand the
   concepts and procedures for operating a domain server, as described
   in RFC-1034, and make sure that the service provided is reliable and
   uninterrupted.  It is his responsibility or that of his delegate to
   ensure that the data will be current at all times.  As a manager, the
   DA must be able to handle complaints about service provided by his
   domain name server.  He must be aware of the behavior of the hosts in
   his domain, and take prompt action on reports of problems, such as
   protocol violations or other serious misbehavior.  The administrator
   of a domain must be a responsible person who has the authority to
   either enforce these actions himself or delegate them to someone
   else.

   Name assignments within a domain are controlled by the DA, who should
   verify that names are unique within his domain and that they conform
   to standard naming conventions.  He furnishes access to names and
   name-related information to users both inside and outside his domain.
   He should work closely with the personnel he has designated as the
   "technical and zone" contacts for his domain, for many administrative
   decisions will be made on the basis of input from these people.

THE DOMAIN TECHNICAL AND ZONE CONTACT

   A zone consists of those contiguous parts of the domain tree for
   which a domain server has complete information and over which it has
   authority.  A domain server may be authoritative for more than one
   zone.  The domain technical/zone contact is the person who tends to
   the technical aspects of maintaining the domain's name server and
   resolver software, and database files.  He keeps the name server
   running, and interacts with technical people in other domains and
   zones to solve problems that affect his zone.

POLICIES

   Domain or host name choices and the allocation of domain name space
   are considered to be local matters.  In the event of conflicts, it is
   the policy of the NIC not to get involved in local disputes or in the
   local decision-making process.  The NIC will not act as referee in
   disputes over such matters as who has the "right" to register a
   particular top-level or second-level domain for an organization.  The
   NIC considers this a private local matter that must be settled among



Stahl                                                           [Page 2]

RFC 1032              DOMAIN ADMINISTRATORS GUIDE          November 1987


   the parties involved prior to their commencing the registration
   process with the NIC.  Therefore, it is assumed that the responsible
   person for a domain will have resolved any local conflicts among the
   members of his domain before registering that domain with the NIC.
   The NIC will give guidance, if requested, by answering specific
   technical questions, but will not provide arbitration in disputes at
   the local level.  This policy is also in keeping with the distributed
   hierarchical nature of the domain-naming system in that it helps to
   distribute the tasks of solving problems and handling questions.

   Naming conventions for hosts should follow the rules specified in
   RFC-952.  From a technical standpoint, domain names can be very long.
   Each segment of a domain name may contain up to 64 characters, but
   the NIC strongly advises DAs to choose names that are 12 characters
   or fewer, because behind every domain system there is a human being
   who must keep track of the names, addresses, contacts, and other data
   in a database.  The longer the name, the more likely the data
   maintainer is to make a mistake.  Users also will appreciate shorter
   names.  Most people agree that short names are easier to remember and
   type; most domain names registered so far are 12 characters or fewer.

   Domain name assignments are made on a first-come-first-served basis.
   The NIC has chosen not to register individual hosts directly under
   the top-level domains it administers.  One advantage of the domain
   naming system is that administration and data maintenance can be
   delegated down a hierarchical tree.  Registration of hosts at the
   same level in the tree as a second-level domain would dilute the
   usefulness of this feature.  In addition, the administrator of a
   domain is responsible for the actions of hosts within his domain.  We
   would not want to find ourselves in the awkward position of policing
   the actions of individual hosts.  Rather, the subdomains registered
   under these top-level domains retain the responsibility for this
   function.

   Countries that wish to be registered as top-level domains are
   required to name themselves after the two-letter country code listed
   in the international standard ISO-3166.  In some cases, however, the
   two-letter ISO country code is identical to a state code used by the
   U.S. Postal Service.  Requests made by countries to use the three-
   letter form of country code specified in the ISO-3166 standard will
   be considered in such cases so as to prevent possible conflicts and
   confusion.









Stahl                                                           [Page 3]

RFC 1032              DOMAIN ADMINISTRATORS GUIDE          November 1987


HOW TO REGISTER

   Obtain a domain questionnaire from the NIC hostmaster, or FTP the
   file NETINFO:DOMAIN-TEMPLATE.TXT from host SRI-NIC.ARPA.

   Fill out the questionnaire completely.  Return it via electronic mail
   to HOSTMASTER@SRI-NIC.ARPA.

   The APPENDIX to this memo contains the application form for
   registering a top-level or second-level domain with the NIC.  It
   supersedes the version of the questionnaire found in RFC-920.  The
   application should be submitted by the person administratively
   responsible for the domain, and must be filled out completely before
   the NIC will authorize establishment of a top-level or second-level
   domain.  The DA is responsible for keeping his domain's data current
   with the NIC or with the registration agent with which his domain is
   registered.  For example, the CSNET and UUCP managements act as
   domain filters, processing domain applications for their own
   organizations.  They pass pertinent information along periodically to
   the NIC for incorporation into the domain database and root server
   files.  The online file NETINFO:ALTERNATE-DOMAIN-PROCEDURE.TXT
   outlines this procedure.  It is highly recommended that the DA review
   this information periodically and provide any corrections or
   additions.  Corrections should be submitted via electronic mail.

WHICH DOMAIN NAME?

   The designers of the domain-naming system initiated several general
   categories of names as top-level domain names, so that each could
   accommodate a variety of organizations.  The current top-level
   domains registered with the DDN Network Information Center are ARPA,
   COM, EDU, GOV, MIL, NET, and ORG, plus a number of top-level country
   domains.  To join one of these, a DA needs to be aware of the purpose
   for which it was intended.

      "ARPA" is a temporary domain.  It is by default appended to the
      names of hosts that have not yet joined a domain.  When the system
      was begun in 1984, the names of all hosts in the Official DoD
      Internet Host Table maintained by the NIC were changed by adding
      of the label ".ARPA" in order to accelerate a transition to the
      domain-naming system.  Another reason for the blanket name changes
      was to force hosts to become accustomed to using the new style
      names and to modify their network software, if necessary.  This
      was done on a network-wide basis and was directed by DCA in DDN
      Management Bulletin No. 22.  Hosts that fall into this domain will
      eventually move to other branches of the domain tree.





Stahl                                                           [Page 4]

RFC 1032              DOMAIN ADMINISTRATORS GUIDE          November 1987


      "COM" is meant to incorporate subdomains of companies and
      businesses.

      "EDU" was initiated to accommodate subdomains set up by
      universities and other educational institutions.

      "GOV" exists to act as parent domain for subdomains set up by
      government agencies.

      "MIL" was initiated to act as parent to subdomains that are
      developed by military organizations.

      "NET" was introduced as a parent domain for various network-type
      organizations.  Organizations that belong within this top-level
      domain are generic or network-specific, such as network service
      centers and consortia.  "NET" also encompasses network
      management-related organizations, such as information centers and
      operations centers.

      "ORG" exists as a parent to subdomains that do not clearly fall
      within the other top-level domains.  This may include technical-
      support groups, professional societies, or similar organizations.

   One of the guidelines in effect in the domain-naming system is that a
   host should have only one name regardless of what networks it is
   connected to.  This implies, that, in general, domain names should
   not include routing information or addresses.  For example, a host
   that has one network connection to the Internet and another to BITNET
   should use the same name when talking to either network.  For a
   description of the syntax of domain names, please refer to Section 3
   of RFC-1034.

VERIFICATION OF DATA

   The verification process can be accomplished in several ways.  One of
   these is through the NIC WHOIS server.  If he has access to WHOIS,
   the DA can type the command "whois domain <domain name><return>".
   The reply from WHOIS will supply the following: the name and address
   of the organization "owning" the domain; the name of the domain; its
   administrative, technical, and zone contacts; the host names and
   network addresses of sites providing name service for the domain.










Stahl                                                           [Page 5]

RFC 1032              DOMAIN ADMINISTRATORS GUIDE          November 1987


         Example:

         @whois domain rice.edu<Return>

            Rice University (RICE-DOM)
            Advanced Studies and Research
            Houston, TX 77001

            Domain Name: RICE.EDU

               Administrative Contact:
               Kennedy, Ken  (KK28)  Kennedy@LLL-CRG.ARPA (713) 527-4834
               Technical Contact, Zone Contact:
               Riffle, Vicky R.  (VRR)  rif@RICE.EDU
               (713) 527-8101 ext 3844

            Domain servers:

            RICE.EDU                     128.42.5.1
            PENDRAGON.CS.PURDUE.EDU      128.10.2.5


   Alternatively, the DA can send an electronic mail message to
   SERVICE@SRI-NIC.ARPA.  In the subject line of the message header, the
   DA should type "whois domain <domain name>".  The requested
   information will be returned via electronic mail.  This method is
   convenient for sites that do not have access to the NIC WHOIS
   service.

   The initial application for domain authorization should be submitted
   via electronic mail, if possible, to HOSTMASTER@SRI-NIC.ARPA.  The
   questionnaire described in the appendix may be used or a separate
   application can be FTPed from host SRI-NIC.ARPA.  The information
   provided by the administrator will be reviewed by hostmaster
   personnel for completeness.  There will most likely be a few
   exchanges of correspondence via electronic mail, the preferred method
   of communication, prior to authorization of the domain.

HOW TO GET MORE INFORMATION

   An informational table of the top-level domains and their root
   servers is contained in the file NETINFO:DOMAINS.TXT online at SRI-
   NIC.ARPA. This table can be obtained by FTPing the file.
   Alternatively, the information can be acquired by opening a TCP or
   UDP connection to the NIC Host Name Server, port 101 on SRI-NIC.ARPA,
   and invoking the command "ALL-DOM".





Stahl                                                           [Page 6]

RFC 1032              DOMAIN ADMINISTRATORS GUIDE          November 1987


   The following online files, all available by FTP from SRI-NIC.ARPA,
   contain pertinent domain information:

      - NETINFO:DOMAINS.TXT, a table of all top-level domains and the
        network addresses of the machines providing domain name
        service for them.  It is updated each time a new top-level
        domain is approved.

      - NETINFO:DOMAIN-INFO.TXT contains a concise list of all
        top-level and second-level domain names registered with the
        NIC and is updated monthly.

      - NETINFO:DOMAIN-CONTACTS.TXT also contains a list of all the
        top level and second-level domains, but includes the
        administrative, technical and zone contacts for each as well.

      - NETINFO:DOMAIN-TEMPLATE.TXT contains the questionnaire to be
        completed before registering a top-level or second-level
        domain.

   For either general or specific information on the domain system, do
   one or more of the following:

      1. Send electronic mail to HOSTMASTER@SRI-NIC.ARPA

      2. Call the toll-free NIC hotline at (800) 235-3155

      3. Use FTP to get background RFCs and other files maintained
         online at the NIC.  Some pertinent RFCs are listed below in
         the REFERENCES section of this memo.





















Stahl                                                           [Page 7]

RFC 1032              DOMAIN ADMINISTRATORS GUIDE          November 1987


REFERENCES

   The references listed here provide important background information
   on the domain-naming system.  Path names of the online files
   available via anonymous FTP from the SRI-NIC.ARPA host are noted in
   brackets.

      1. Defense Communications Agency DDN Defense Communications
         System, DDN Management Bulletin No. 22, Domain Names
         Transition, March 1984.
         [ DDN-NEWS:DDN-MGT-BULLETIN-22.TXT ]

      2. Defense Communications Agency DDN Defense Communications
         System, DDN Management Bulletin No. 32, Phase I of the Domain
         Name Implementation, January 1987.
         [ DDN-NEWS:DDN-MGT-BULLETIN-32.TXT ]

      3. Harrenstien, K., M. Stahl, and E. Feinler, "Hostname
         Server", RFC-953, DDN Network Information Center, SRI
         International, October 1985.  [ RFC:RFC953.TXT ]

      4. Harrenstien, K., M. Stahl, and E. Feinler, "Official DoD
         Internet Host Table Specification", RFC-952, DDN Network
         Information Center, SRI International, October 1985.
         [ RFC:RFC952.TXT ]

      5. ISO, "Codes for the Representation of Names of Countries",
         ISO-3166, International Standards Organization, May 1981.
         [ Not online ]

      6. Lazear, W.D., "MILNET Name Domain Transition", RFC-1031,
         Mitre Corporation, October 1987.  [ RFC:RFC1031.TXT ]

      7. Lottor, M.K., "Domain Administrators Operations Guide",
         RFC-1033, DDN Network Information Center, SRI International,
         July 1987.  [ RFC:RFC1033.TXT ]

      8. Mockapetris, P., "Domain Names - Concepts and Facilities",
         RFC-1034, USC Information Sciences Institute, October 1987.
         [ RFC:RFC1034.TXT ]

      9. Mockapetris, P., "Domain Names - Implementation and
         Specification", RFC-1035, USC Information Sciences Institute,
         October 1987.  [ RFC:RFC1035.TXT ]

     10. Mockapetris, P., "The Domain Name System", Proceedings of the
         IFIP 6.5 Working Conference on Computer Message Services,
         Nottingham, England, May 1984.  Also as ISI/RS-84-133, June



Stahl                                                           [Page 8]

RFC 1032              DOMAIN ADMINISTRATORS GUIDE          November 1987


         1984.  [ Not online ]

     11. Mockapetris, P., J. Postel, and P. Kirton, "Name Server
         Design for Distributed Systems", Proceedings of the Seventh
         International Conference on Computer Communication, October
         30 to November 3 1984, Sidney, Australia.  Also as
         ISI/RS-84-132, June 1984.  [ Not online ]

     12. Partridge, C., "Mail Routing and the Domain System", RFC-974,
         CSNET-CIC, BBN Laboratories, January 1986.
         [ RFC:RFC974.TXT ]

     13. Postel, J., "The Domain Names Plan and Schedule", RFC-881,
         USC Information Sciences Institute, November 1983.
         [ RFC:RFC881.TXT ]

     14. Reynolds, J., and Postel, J., "Assigned Numbers", RFC-1010
         USC Information Sciences Institute, May 1986.
         [ RFC:RFC1010.TXT ]

     15. Romano, S., and Stahl, M., "Internet Numbers", RFC-1020,
         SRI, November 1987.
         [ RFC:RFC1020.TXT ]




























Stahl                                                           [Page 9]

RFC 1032              DOMAIN ADMINISTRATORS GUIDE          November 1987


APPENDIX

   The following questionnaire may be FTPed from SRI-NIC.ARPA as
   NETINFO:DOMAIN-TEMPLATE.TXT.

   ---------------------------------------------------------------------

   To establish a domain, the following information must be sent to the
   NIC Domain Registrar (HOSTMASTER@SRI-NIC.ARPA):

   NOTE: The key people must have electronic mailboxes and NIC
   "handles," unique NIC database identifiers.  If you have access to
   "WHOIS", please check to see if you are registered and if so, make
   sure the information is current.  Include only your handle and any
   changes (if any) that need to be made in your entry.  If you do not
   have access to "WHOIS", please provide all the information indicated
   and a NIC handle will be assigned.

   (1)  The name of the top-level domain to join.

         For example:  COM

   (2) The NIC handle of the administrative head of the organization.
   Alternately, the person's name, title, mailing address, phone number,
   organization, and network mailbox.  This is the contact point for
   administrative and policy questions about the domain.  In the case of
   a research project, this should be the principal investigator.

         For example:

            Administrator

               Organization  The NetWorthy Corporation
               Name          Penelope Q. Sassafrass
               Title         President
               Mail Address  The NetWorthy Corporation
                             4676 Andrews Way, Suite 100
                             Santa Clara, CA 94302-1212
               Phone Number  (415) 123-4567
               Net Mailbox   Sassafrass@ECHO.TNC.COM
               NIC Handle    PQS

   (3)  The NIC handle of the technical contact for the domain.
   Alternately, the person's name, title, mailing address, phone number,
   organization, and network mailbox.  This is the contact point for
   problems concerning the domain or zone, as well as for updating
   information about the domain or zone.




Stahl                                                          [Page 10]

RFC 1032              DOMAIN ADMINISTRATORS GUIDE          November 1987


         For example:

            Technical and Zone Contact

               Organization  The NetWorthy Corporation
               Name          Ansel A. Aardvark
               Title         Executive Director
               Mail Address  The NetWorthy Corporation
                             4676 Andrews Way, Suite 100
                             Santa Clara, CA. 94302-1212
               Phone Number  (415) 123-6789
               Net Mailbox   Aardvark@ECHO.TNC.COM
               NIC Handle    AAA2

   (4)  The name of the domain (up to 12 characters).  This is the name
   that will be used in tables and lists associating the domain with the
   domain server addresses.  [While, from a technical standpoint, domain
   names can be quite long (programmers beware), shorter names are
   easier for people to cope with.]

         For example:  TNC

   (5)  A description of the servers that provide the domain service for
   translating names to addresses for hosts in this domain, and the date
   they will be operational.

         A good way to answer this question is to say "Our server is
         supplied by person or company X and does whatever their standard
         issue server does."

            For example:  Our server is a copy of the one operated by
            the NIC; it will be installed and made operational on
            1 November 1987.

   (6) Domains must provide at least two independent servers for the
   domain.  Establishing the servers in physically separate locations
   and on different PSNs is strongly recommended.  A description of the
   server machine and its backup, including













Stahl                                                          [Page 11]

RFC 1032              DOMAIN ADMINISTRATORS GUIDE          November 1987


         (a) Hardware and software (using keywords from the Assigned
         Numbers RFC).

         (b) Host domain name and network addresses (which host on which
         network for each connected network).

         (c) Any domain-style nicknames (please limit your domain-style
         nickname request to one)

         For example:

            - Hardware and software

               VAX-11/750  and  UNIX,    or
               IBM-PC      and  MS-DOS,  or
               DEC-1090    and  TOPS-20

            - Host domain names and network addresses

               BAR.FOO.COM 10.9.0.193 on ARPANET

            - Domain-style nickname

               BR.FOO.COM (same as BAR.FOO.COM 10.9.0.13 on ARPANET)

   (7)  Planned mapping of names of any other network hosts, other than
   the server machines, into the new domain's naming space.

         For example:

            BAR-FOO2.ARPA (10.8.0.193) -> FOO2.BAR.COM
            BAR-FOO3.ARPA (10.7.0.193) -> FOO3.BAR.COM
            BAR-FOO4.ARPA (10.6.0.193) -> FOO4.BAR.COM


   (8)  An estimate of the number of hosts that will be in the domain.

         (a) Initially
         (b) Within one year
         (c) Two years
         (d) Five years.

         For example:

            (a) Initially  =   50
            (b) One year   =  100
            (c) Two years  =  200
            (d) Five years =  500



Stahl                                                          [Page 12]

RFC 1032              DOMAIN ADMINISTRATORS GUIDE          November 1987


   (9)  The date you expect the fully qualified domain name to become
   the official host name in HOSTS.TXT.

         Please note: If changing to a fully qualified domain name (e.g.,
         FOO.BAR.COM) causes a change in the official host name of an
         ARPANET or MILNET host, DCA approval must be obtained beforehand.
         Allow 10 working days for your requested changes to be processed.

         ARPANET sites should contact ARPANETMGR@DDN1.ARPA.  MILNET sites
         should contact HOSTMASTER@SRI-NIC.ARPA, 800-235-3155, for
         further instructions.

   (10) Please describe your organization briefly.

         For example: The NetWorthy Corporation is a consulting
         organization of people working with UNIX and the C language in an
         electronic networking environment.  It sponsors two technical
         conferences annually and distributes a bimonthly newsletter.

   ---------------------------------------------------------------------

   This example of a completed application corresponds to the examples
   found in the companion document RFC-1033, "Domain Administrators
   Operations Guide."

   (1)  The name of the top-level domain to join.

            COM

   (2)  The NIC handle of the administrative contact person.

            NIC Handle    JAKE

   (3)  The NIC handle of the domain's technical and zone
         contact person.

            NIC Handle    DLE6

   (4)  The name of the domain.

            SRI

   (5)  A description of the servers.

            Our server is the TOPS20 server JEEVES supplied by ISI; it
            will be installed and made operational on 1 July 1987.





Stahl                                                          [Page 13]

RFC 1032              DOMAIN ADMINISTRATORS GUIDE          November 1987


   (6)  A description of the server machine and its backup:

            (a) Hardware and software

               DEC-1090T   and  TOPS20
               DEC-2065    and  TOPS20

            (b) Host domain name and network address

               KL.SRI.COM  10.1.0.2 on ARPANET, 128.18.10.6 on SRINET
               STRIPE.SRI.COM  10.4.0.2 on ARPANET, 128.18.10.4 on SRINET

            (c) Domain-style nickname

               None

   (7)  Planned mapping of names of any other network hosts, other than
   the server machines, into the new domain's naming space.

            SRI-Blackjack.ARPA (128.18.2.1) -> Blackjack.SRI.COM
            SRI-CSL.ARPA (192.12.33.2) -> CSL.SRI.COM

   (8)  An estimate of the number of hosts that will be directly within
   this domain.

            (a) Initially  =   50
            (b) One year   =  100
            (c) Two years  =  200
            (d) Five years =  500

   (9)  A date when you expect the fully qualified domain name to become
   the official host name in HOSTS.TXT.

            31 September 1987

   (10)  Brief description of organization.

            SRI International is an independent, nonprofit, scientific
            research organization.  It performs basic and applied research
            for government and commercial clients, and contributes to
            worldwide economic, scientific, industrial, and social progress
            through research and related services.









Stahl                                                          [Page 14]

