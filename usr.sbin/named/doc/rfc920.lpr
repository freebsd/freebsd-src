

Network Working Group                                          J. Postel
Request for Comments: 920                                    J. Reynolds
                                                                     ISI
                                                            October 1984

                          Domain Requirements


Status of this Memo

   This memo is a policy statement on the requirements of establishing a
   new domain in the ARPA-Internet and the DARPA research community.
   This is an official policy statement of the IAB and the DARPA.
   Distribution of this memo is unlimited.

Introduction

   This memo restates and refines the requirements on establishing a
   Domain first described in RFC-881 [1].  It adds considerable detail
   to that discussion, and introduces the limited set of top level
   domains.

The Purpose of Domains

   Domains are administrative entities.  The purpose and expected use of
   domains is to divide the name management required of a central
   administration and assign it to sub-administrations.  There are no
   geographical, topological, or technological constraints on a domain.
   The hosts in a domain need not have common hardware or software, nor
   even common protocols.  Most of the requirements and limitations on
   domains are designed to ensure responsible administration.

   The domain system is a tree-structured global name space that has a
   few top level domains.  The top level domains are subdivided into
   second level domains.  The second level domains may be subdivided
   into third level domains, and so on.

   The administration of a domain requires controlling the assignment of
   names within that domain and providing access to the names and name
   related information (such as addresses) to users both inside and
   outside the domain.












Postel & Reynolds                                               [Page 1]



RFC 920                                                     October 1984
Domain Requirements


General Purpose Domains

   While the initial domain name "ARPA" arises from the history of the
   development of this system and environment, in the future most of the
   top level names will be very general categories like "government",
   "education", or "commercial".  The motivation is to provide an
   organization name that is free of undesirable semantics.

   After a short period of initial experimentation, all current
   ARPA-Internet hosts will select some domain other than ARPA for their
   future use.  The use of ARPA as a top level domain will eventually
   cease.

Initial Set of Top Level Domains

   The initial top level domain names are:

      Temporary

         ARPA  =  The current ARPA-Internet hosts.

      Categories

         GOV  =  Government, any government related domains meeting the
                 second level requirements.

         EDU  =  Education, any education related domains meeting the
                 second level requirements.

         COM  =  Commercial, any commercial related domains meeting the
                 second level requirements.

         MIL  =  Military, any military related domains meeting the
                 second level requirements.

         ORG  =  Organization, any other domains meeting the second
                 level requirements.

      Countries

         The English two letter code (alpha-2) identifying a country
         according the the ISO Standard for "Codes for the
         Representation of Names of Countries" [5].






Postel & Reynolds                                               [Page 2]



RFC 920                                                     October 1984
Domain Requirements


      Multiorganizations

         A multiorganization may be a top level domain if it is large,
         and is composed of other organizations; particularly if the
         multiorganization can not be easily classified into one of the
         categories and is international in scope.

Possible Examples of Domains

   The following examples are fictions of the authors' creation, any
   similarity to the real world is coincidental.

   The UC Domain

      It might be that a large state wide university with, say, nine
      campuses and several laboratories may want to form a domain.  Each
      campus or major off-campus laboratory might then be a subdomain,
      and within each subdomain, each department could be further
      distinguished.  This university might be a second level domain in
      the education category.

      One might see domain style names for hosts in this domain like
      these:

         LOCUS.CS.LA.UC.EDU
         CCN.OAC.LA.UC.EDU
         ERNIE.CS.CAL.UC.EDU
         A.S1.LLNL.UC.EDU
         A.LAND.LANL.UC.EDU
         NMM.LBL.CAL.UC.EDU

   The MIT Domain

      Another large university may have many hosts using a variety of
      machine types, some even using several families of protocols.
      However, the administrators at this university may see no need for
      the outside world to be aware of these internal differences.  This
      university might be a second level domain in the education
      category.

      One might see domain style names for hosts in this domain like
      these:

         APIARY-1.MIT.EDU
         BABY-BLUE.MIT.EDU
         CEZANNE.MIT.EDU
         DASH.MIT.EDU


Postel & Reynolds                                               [Page 3]



RFC 920                                                     October 1984
Domain Requirements


         MULTICS.MIT.EDU
         TAC.MIT.EDU
         XX.MIT.EDU

   The CSNET Domain

      There may be a consortium of universities and industry research
      laboratories called, say, "CSNET".  This CSNET is not a network
      per se, but rather a computer mail exchange using a variety of
      protocols and network systems.  Therefore, CSNET is not a network
      in the sense of the ARPANET, or an Ethernet, or even the
      ARPA-Internet, but rather a community.  Yet it does, in fact, have
      the key property needed to form a domain; it has a responsible
      administration.  This consortium might be large enough and might
      have membership that cuts across the categories in such a way that
      it qualifies under the "multiorganization rule" to be a top level
      domain.

      One might see domain style names for hosts in this domain like
      these:

         CIC.CSNET
         EMORY.CSNET
         GATECH.CSNET
         HP-LABS.CSNET
         SJ.IBM.CSNET
         UDEL.CSNET
         UWISC.CSNET

General Requirements on a Domain

   There are several requirements that must be met to establish a
   domain.  In general, it must be responsibly managed.  There must be a
   responsible person to serve as an authoritative coordinator for
   domain related questions.  There must be a robust domain name lookup
   service, it must be of at least a minimum size, and the domain must
   be registered with the central domain administrator (the Network
   Information Center (NIC) Domain Registrar).

   Responsible Person:

      An individual must be identified who has authority for the
      administration of the names within the domain, and who seriously
      takes on the responsibility for the behavior of the hosts in the
      domain, plus their interactions with hosts outside the domain.
      This person must have some technical expertise and the authority
      within the domain to see that problems are fixed.


Postel & Reynolds                                               [Page 4]



RFC 920                                                     October 1984
Domain Requirements


      If a host in a given domain somehow misbehaves in its interactions
      with hosts outside the domain (e.g., consistently violates
      protocols), the responsible person for the domain must be
      competent and available to receive reports of problems, take
      action on the reported problems, and follow through to eliminate
      the problems.

   Domain Servers:

      A robust and reliable domain server must be provided.  One way of
      meeting this requirement is to provide at least two independent
      domain servers for the domain.  The database can, of course, be
      the same.  The database can be prepared and copied to each domain
      server.  But, the servers should be in separate machines on
      independent power supplies, et cetera; basically as physically
      independent as can be.  They should have no common point of
      failure.

      Some domains may find that providing a robust domain service can
      most easily be done by cooperating with another domain where each
      domain provides an additional server for the other.

      In other situations, it may be desirable for a domain to arrange
      for domain service to be provided by a third party, perhaps on
      hosts located outside the domain.

      One of the difficult problems in operating a domain server is the
      acquisition and maintenance of the data.  In this case, the data
      are the host names and addresses.  In some environments this
      information changes fairly rapidly and keeping up-to-date data may
      be difficult.  This is one motivation for sub-domains.  One may
      wish to create sub-domains until the rate of change of the data in
      a sub-domain domain server database is easily managed.

      In the technical language of the domain server implementation the
      data is divided into zones.  Domains and zones are not necessarily
      one-to-one.  It may be reasonable for two or more domains to
      combine their data in a single zone.

      The responsible person or an identified technical assistant must
      understand in detail the procedures for operating a domain server,
      including the management of master files and zones.

      The operation of a domain server should not be taken on lightly.
      There are some difficult problems in providing an adequate
      service, primarily the problems in keeping the database up to
      date, and keeping the service operating.


Postel & Reynolds                                               [Page 5]



RFC 920                                                     October 1984
Domain Requirements


      The concepts and implementation details of the domain server are
      given in RFC-882 [2] and RFC-883 [3].

   Minimum Size:

      The domain must be of at least a minimum size.  There is no
      requirement to form a domain because some set of hosts is above
      the minimum size.

      Top level domains must be specially authorized.  In general, they
      will only be authorized for domains expected to have over 500
      hosts.

      The general guideline for a second level domain is that it have
      over 50 hosts.  This is a very soft "requirement".  It makes sense
      that any major organization, such as a university or corporation,
      be allowed as a second level domain -- even if it has just a few
      hosts.

   Registration:

      Top level domains must be specially authorized and registered with
      the NIC domain registrar.

      The administrator of a level N domain must register with the
      registrar (or responsible person) of the level N-1 domain.  This
      upper level authority must be satisfied that the requirements are
      met before authorization for the domain is granted.

      The registration procedure involves answering specific questions
      about the prospective domain.  A prototype of what the NIC Domain
      Registrar may ask for the registration of a second level domain is
      shown below.  These questions may change from time to time.  It is
      the responsibility of domain administrators to keep this
      information current.

      The administrator of a domain is required to make sure that host
      and sub-domain names within that jurisdiction conform to the
      standard name conventions and are unique within that domain.

      If sub-domains are set up, the administrator may wish to pass
      along some of his authority and responsibility to a sub-domain
      administrator.  Even if sub-domains are established, the
      responsible person for the top-level domain is ultimately
      responsible for the whole tree of sub-domains and hosts.

      This does not mean that a domain administrator has to know the


Postel & Reynolds                                               [Page 6]



RFC 920                                                     October 1984
Domain Requirements


      details of all the sub-domains and hosts to the Nth degree, but
      simply that if a problem occurs he can get it fixed by calling on
      the administrator of the sub-domain containing the problem.

Top Level Domain Requirements

   There are very few top level domains, each of these may have many
   second level domains.

   An initial set of top level names has been identified.  Each of these
   has an administrator and an agent.

   The top level domains:

      ARPA =  The ARPA-Internet   *** TEMPORARY ***

         Administrator:  DARPA
         Agent:          The Network Information Center
         Mailbox:        HOSTMASTER@SRI-NIC.ARPA

      GOV  =  Government

         Administrator:  DARPA
         Agent:          The Network Information Center
         Mailbox:        HOSTMASTER@SRI-NIC.ARPA

      EDU  =  Education

         Administrator:  DARPA
         Agent:          The Network Information Center
         Mailbox:        HOSTMASTER@SRI-NIC.ARPA

      COM  =  Commercial

         Administrator:  DARPA
         Agent:          The Network Information Center
         Mailbox:        HOSTMASTER@SRI-NIC.ARPA

      MIL  =  Military

         Administrator:  DDN-PMO
         Agent:          The Network Information Center
         Mailbox:        HOSTMASTER@SRI-NIC.ARPA






Postel & Reynolds                                               [Page 7]



RFC 920                                                     October 1984
Domain Requirements


      ORG  =  Organization

         Administrator:  DARPA
         Agent:          The Network Information Center
         Mailbox:        HOSTMASTER@SRI-NIC.ARPA

      Countries

         The English two letter code (alpha-2) identifying a country
         according the the ISO Standard for "Codes for the
         Representation of Names of Countries" [5].

         As yet no country domains have been established.  As they are
         established information about the administrators and agents
         will be made public, and will be listed in subsequent editions
         of this memo.

      Multiorganizations

         A multiorganization may be a top level domain if it is large,
         and is composed of other organizations; particularly if the
         multiorganization can not be easily classified into one of the
         categories and is international in scope.

         As yet no multiorganization domains have been established.  As
         they are established information about the administrators and
         agents will be made public, and will be listed in subsequent
         editions of this memo.

      Note:  The NIC is listed as the agent and registrar for all the
      currently allowed top level domains.  If there are other entities
      that would be more appropriate agents and registrars for some or
      all of these domains then it would be desirable to reassign the
      responsibility.

Second Level Domain Requirements

   Each top level domain may have many second level domains.  Every
   second level domain must meet the general requirements on a domain
   specified above, and be registered with a top level domain
   administrator.








Postel & Reynolds                                               [Page 8]



RFC 920                                                     October 1984
Domain Requirements


Third through Nth Level Domain Requirements

   Each second level domain may have many third level domains, etc.
   Every third level domain (through Nth level domain) must meet the
   requirements set by the administrator of the immediately higher level
   domain.  Note that these may be more or less strict than the general
   requirements.  One would expect the minimum size requirements to
   decrease at each level.

The ARPA Domain

   At the time the implementation of the domain concept was begun it was
   thought that the set of hosts under the administrative authority of
   DARPA would make up a domain.  Thus the initial domain selected was
   called ARPA.  Now it is seen that there is no strong motivation for
   there to be a top level ARPA domain.  The plan is for the current
   ARPA domain to go out of business as soon as possible.  Hosts that
   are currently members of the ARPA domain should make arrangements to
   join another domain.  It is likely that for experimental purposes
   there will be a second level domain called ARPA in the ORG domain
   (i.e., there will probably be an ARPA.ORG domain).

The DDN Hosts

   DDN hosts that do not desire to participate in this domain naming
   system will continue to use the HOSTS.TXT data file maintained by the
   NIC for name to address translations.  This file will be kept up to
   date for the DDN hosts.  However, all DDN hosts will change their
   names from "host.ARPA" to (for example) "host.DDN.MIL" some time in
   the future.  The schedule for changes required in DDN hosts will be
   established by the DDN-PMO.

Impact on Hosts

   What is a host administrator to do about all this?

      For existing hosts already operating in the ARPA-Internet, the
      best advice is to sit tight for now.  Take a few months to
      consider the options, then select a domain to join.  Plan
      carefully for the impact that changing your host name will have on
      both your local users and on their remote correspondents.

      For a new host, careful thought should be given (as discussed
      below).  Some guidance can be obtained by comparing notes on what
      other hosts with similar administrative properties have done.

   The owner of a host may decide which domain to join, and the


Postel & Reynolds                                               [Page 9]



RFC 920                                                     October 1984
Domain Requirements


   administrator of a domain may decide which hosts to accept into his
   domain.  Thus the owner of a host and a domain administrator must
   come to an understanding about the host being in the domain.  This is
   the foundation of responsible administration.

      For example, a host "XYZ" at MIT might possible be considered as a
      candidate for becoming any of XYZ.ARPA.ORG, XYZ.CSNET, or
      XYZ.MIT.EDU.

         The owner of host XYZ may choose which domain to join,
         depending on which domain administrators are willing to have
         him.

   The domain is part of the host name.  Thus if USC-ISIA.ARPA changes
   its domain affiliation to DDN.MIL to become USC-ISIA.DDN.MIL, it has
   changed its name.  This means that any previous references to
   USC-ISIA.ARPA are now out of date.  Such old references may include
   private host name to address tables, and any recorded information
   about mailboxes such as mailing lists, the headers of old messages,
   printed directories, and peoples' memories.

   The experience of the DARPA community suggests that changing the name
   of a host is somewhat painful.  It is recommended that careful
   thought be given to choosing a new name for a host - which includes
   selecting its place in the domain hierarchy.

The Roles of the Network Information Center

   The NIC plays two types of roles in the administration of domains.
   First,  the NIC is the registrar of all top level domains.  Second
   the NIC is the administrator of several top level domains (and the
   registrar for second level domains in these).

   Top Level Domain Registrar

      As the registrar for top level domains, the NIC is the contact
      point for investigating the possibility of establishing a new top
      level domain.

   Top Level Domain Administrator

      For the top level domains designated so far, the NIC is the
      administrator of each of these domains.  This means the NIC is
      responsible for the management of these domains and the
      registration of the second level domains or hosts (if at the
      second level) in these domains.



Postel & Reynolds                                              [Page 10]



RFC 920                                                     October 1984
Domain Requirements


      It may be reasonable for the administration of some of these
      domains to be taken on by other authorities in the future.  It is
      certainly not desired that the NIC be the administrator of all top
      level domains forever.

Prototypical Questions

   To establish a domain, the following information must be provided to
   the NIC Domain Registrar (HOSTMASTER@SRI-NIC.ARPA):

      Note:  The key people must have computer mail mailboxes and
      NIC-Idents.  If they do not at present, please remedy the
      situation at once.  A NIC-Ident may be established by contacting
      NIC@SRI-NIC.ARPA.

   1)  The name of the top level domain to join.

      For example:  EDU

   2)  The name, title, mailing address, phone number, and organization
   of the administrative head of the organization.  This is the contact
   point for administrative and policy questions about the domain.  In
   the case of a research project, this should be the Principal
   Investigator.  The online mailbox and NIC-Ident of this person should
   also be included.

      For example:

         Administrator

            Organization  USC/Information Sciences Institute
            Name          Keith Uncapher
            Title         Executive Director
            Mail Address  USC/ISI
                          4676 Admiralty Way, Suite 1001
                          Marina del Rey, CA. 90292-6695
            Phone Number  213-822-1511
            Net Mailbox   Uncapher@USC-ISIB.ARPA
            NIC-Ident     KU

   3)  The name, title, mailing address, phone number, and organization
   of the domain technical contact.  The online mailbox and NIC-Ident of
   the domain technical contact should also be included.  This is the
   contact point for problems with the domain and for updating
   information about the domain.  Also, the domain technical contact may
   be responsible for hosts in this domain.



Postel & Reynolds                                              [Page 11]



RFC 920                                                     October 1984
Domain Requirements


      For example:

         Technical Contact

            Organization  USC/Information Sciences Institute
            Name          Craig Milo Rogers
            Title         Researcher
            Mail Address  USC/ISI
                          4676 Admiralty Way, Suite 1001
                          Marina del Rey, CA. 90292-6695
            Phone Number  213-822-1511
            Net Mailbox   Rogers@USC-ISIB.ARPA
            NIC-Ident     CMR

   4)  The name, title, mailing address, phone number, and organization
   of the zone technical contact.  The online mailbox and NIC-Ident of
   the zone technical contact should also be included.  This is the
   contact point for problems with the zone and for updating information
   about the zone.  In many cases the zone technical contact and the
   domain technical contact will be the same person.

      For example:

         Technical Contact

            Organization  USC/Information Sciences Institute
            Name          Craig Milo Rogers
            Title         Researcher
            Mail Address  USC/ISI
                          4676 Admiralty Way, Suite 1001
                          Marina del Rey, CA. 90292-6695
            Phone Number  213-822-1511
            Net Mailbox   Rogers@USC-ISIB.ARPA
            NIC-Ident     CMR

   5)  The name of the domain (up to 12 characters).  This is the name
   that will be used in tables and lists associating the domain and the
   domain server addresses.  [While technically domain names can be
   quite long (programmers beware), shorter names are easier for people
   to cope with.]

      For example:  ALPHA-BETA

   6)  A description of the servers that provides the domain service for
   translating name to address for hosts in this domain, and the date
   they will be operational.



Postel & Reynolds                                              [Page 12]



RFC 920                                                     October 1984
Domain Requirements


      A good way to answer this question is to say "Our server is
      supplied by person or company X and does whatever their standard
      issue server does".

         For example:  Our server is a copy of the server operated by
         the NIC, and will be installed and made operational on
         1-November-84.

   7)  A description of the server machines, including:

      (a) hardware and software (using keywords from the Assigned
      Numbers)

      (b) addresses (what host on what net for each connected net)

      For example:

         (a) hardware and software

            VAX-11/750  and  UNIX,    or
            IBM-PC      and  MS-DOS,  or
            DEC-1090    and  TOPS-20

         (b) address

            10.9.0.193 on ARPANET

   8)  An estimate of the number of hosts that will be in the domain.

      (a) initially,
      (b) within one year,
      (c) two years, and
      (d) five years.

      For example:

         (a) initially  =   50
         (b) one year   =  100
         (c) two years  =  200
         (d) five years =  500









Postel & Reynolds                                              [Page 13]



RFC 920                                                     October 1984
Domain Requirements


Acknowledgment

   We would like to thank the many people who contributed to this memo,
   including the participants in the Namedroppers Group, the ICCB, the
   PCCB, and especially the staff of the Network Information Center,
   particularly J. Feinler and K. Harrenstien.

References

   [1]  Postel, J., "The Domain Names Plan and Schedule", RFC-881, USC
        Information Sciences Institute, November 1983.

   [2]  Mockapetris, P., "Domain Names - Concepts and Facilities",
        RFC-882, USC Information Sciences Institute, November 1983.

   [3]  Mockapetris, P., "Domain Names - Implementation and
        Specification", RFC-883, USC Information Sciences Institute,
        November 1983.

   [4]  Postel, J., "Domain Name System Implementation Schedule",
        RFC-897, USC Information Sciences Institute, February 1984.

   [5]  ISO, "Codes for the Representation of Names of Countries",
        ISO-3166, International Standards Organization, May 1981.

   [6]  Postel, J., "Domain Name System Implementation Schedule -
        Revised", RFC-921, USC Information Sciences Institute, October
        1984.

   [7]  Mockapetris, P., "The Domain Name System", Proceedings of the
        IFIP 6.5 Working Conference on Computer Message Services,
        Nottingham, England, May 1984.  Also as ISI/RS-84-133,
        June 1984.

   [8]  Mockapetris, P., J. Postel, and P. Kirton, "Name Server Design
        for Distributed Systems", Proceedings of the Seventh
        International Conference on Computer Communication, October 30
        to November 3 1984, Sidney, Australia.  Also as ISI/RS-84-132,
        June 1984.










Postel & Reynolds                                              [Page 14]

