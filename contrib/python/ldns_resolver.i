/******************************************************************************
 * ldns_resolver.i: LDNS resolver class
 *
 * Copyright (c) 2009, Zdenek Vasicek (vasicek AT fit.vutbr.cz)
 *                     Karel Slany    (slany AT fit.vutbr.cz)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the organization nor the names of its
 *       contributors may be used to endorse or promote products derived from this
 *       software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************/

%typemap(in,numinputs=0,noblock=1) (ldns_resolver **r)
{
 ldns_resolver *$1_res;
 $1 = &$1_res;
}
          
/* result generation */
%typemap(argout,noblock=1) (ldns_resolver **r)
{
  $result = SWIG_Python_AppendOutput($result, SWIG_NewPointerObj(SWIG_as_voidptr($1_res), SWIGTYPE_p_ldns_struct_resolver, SWIG_POINTER_OWN |  0 ));
}

//TODO: pop_nameserver a podobne funkce musi predat objekt do spravy PYTHONU!!
%newobject ldns_resolver_pop_nameserver;
%newobject ldns_resolver_query;
%newobject ldns_axfr_next;

%delobject ldns_resolver_deep_free;
%delobject ldns_resolver_free;

%nodefaultctor ldns_struct_resolver; //no default constructor & destructor
%nodefaultdtor ldns_struct_resolver;

%ignore ldns_struct_resolver::_searchlist;
%ignore ldns_struct_resolver::_nameservers;
%ignore ldns_resolver_set_nameservers;

%rename(ldns_resolver) ldns_struct_resolver;

#ifdef LDNS_DEBUG
%rename(__ldns_resolver_deep_free) ldns_resolver_deep_free;
%rename(__ldns_resolver_free) ldns_resolver_free;
%inline %{
void _ldns_resolver_free (ldns_resolver* r) {
   printf("******** LDNS_RESOLVER deep free 0x%lX ************\n", (long unsigned int)r);
   ldns_resolver_deep_free(r);
}
%}
#else
%rename(_ldns_resolver_deep_free) ldns_resolver_deep_free;
%rename(_ldns_resolver_free) ldns_resolver_free;
#endif

%feature("docstring") ldns_struct_resolver "LDNS resolver object. 

The ldns_resolver object keeps a list of nameservers and can perform queries.

**Usage**

>>> import ldns
>>> resolver = ldns.ldns_resolver.new_frm_file(\"/etc/resolv.conf\")
>>> pkt = resolver.query(\"www.nic.cz\", ldns.LDNS_RR_TYPE_A,ldns.LDNS_RR_CLASS_IN)
>>> if (pkt) and (pkt.answer()): 
>>>    print pkt.answer()
www.nic.cz.	1757	IN	A	217.31.205.50

This simple example instances a resolver in order to resolve www.nic.cz record of A type. 
"

%extend ldns_struct_resolver {
 
 %pythoncode %{
        def __init__(self):
            raise Exception("This class can't be created directly. Please use: new_frm_file(filename), new_frm_fp(file) or new_frm_fp_l(file,line)")

        __swig_destroy__ = _ldns._ldns_resolver_free

        #LDNS_RESOLVER_CONSTRUCTORS_#
        @staticmethod
        def new_frm_file(filename = "/etc/resolv.conf", raiseException=True):
            """Creates a resolver object from given filename
               
               :param filename: name of file which contains informations (usually /etc/resolv.conf)
               :param raiseException: if True, an exception occurs in case a resolver object can't be created
               :returns: resolver object or None. If the object can't be created and raiseException is True, an exception occurs.
            """
            status, resolver = _ldns.ldns_resolver_new_frm_file(filename)
            if status != LDNS_STATUS_OK:
                if (raiseException): raise Exception("Can't create resolver, error: %d" % status)
                return None
            return resolver

        @staticmethod
        def new_frm_fp(file, raiseException=True):
            """Creates a resolver object from file
               
               :param file: a file object
               :param raiseException: if True, an exception occurs in case a resolver object can't be created
               :returns: resolver object or None. If the object can't be created and raiseException is True, an exception occurs.
            """
            status, resolver = _ldns.ldns_resolver_new_frm_fp(file)
            if status != LDNS_STATUS_OK:
                if (raiseException): raise Exception("Can't create resolver, error: %d" % status)
                return None
            return resolver

        @staticmethod
        def new_frm_fp_l(file, raiseException=True):
            """Creates a resolver object from file
               
               :param file: a file object
               :param raiseException: if True, an exception occurs in case a resolver instance can't be created
               :returns: 
                  * resolver - resolver instance or None. If an instance can't be created and raiseException is True, an exception occurs.

                  * line - the line number (for debugging)
            """
            status, resolver, line = _ldns.ldns_resolver_new_frm_fp_l(file)
            if status != LDNS_STATUS_OK:
                if (raiseException): raise Exception("Can't create resolver, error: %d" % status)
                return None
            return resolver, line

        #_LDNS_RESOLVER_CONSTRUCTORS#

        # High level functions
        def get_addr_by_name(self, name, aclass = _ldns.LDNS_RR_CLASS_IN, flags = _ldns.LDNS_RD):
            """Ask the resolver about name and return all address records

               :param name: (ldns_rdf) the name to look for
               :param aclass: the class to use
               :param flags: give some optional flags to the query

               :returns: RR List object or None

               **Usage**
                 >>> addr = resolver.get_addr_by_name("www.google.com", ldns.LDNS_RR_CLASS_IN, ldns.LDNS_RD)
                 >>> if (not addr): raise Exception("Can't retrieve server address")
                 >>> for rr in addr.rrs():
                 >>>     print rr
                 www.l.google.com.	300	IN	A	74.125.43.99
                 www.l.google.com.	300	IN	A	74.125.43.103
                 www.l.google.com.	300	IN	A	74.125.43.104
                 www.l.google.com.	300	IN	A	74.125.43.147
                    
            """
            rdf = name
            if isinstance(name, str):
                rdf =  _ldns.ldns_dname_new_frm_str(name)
            return _ldns.ldns_get_rr_list_addr_by_name(self, rdf, aclass, flags)

        def get_name_by_addr(self, addr, aclass = _ldns.LDNS_RR_CLASS_IN, flags = _ldns.LDNS_RD):
            """Ask the resolver about the address and return the name

               :param name: (ldns_rdf of A or AAAA type) the addr to look for. If a string is given, A or AAAA type is identified automatically
               :param aclass: the class to use
               :param flags: give some optional flags to the query

               :returns: RR List object or None

               **Usage**
                 >>> addr = resolver.get_name_by_addr("74.125.43.99", ldns.LDNS_RR_CLASS_IN, ldns.LDNS_RD)
                 >>> if (not addr): raise Exception("Can't retrieve server address")
                 >>> for rr in addr.rrs():
                 >>>     print rr
                 99.43.125.74.in-addr.arpa.	85641	IN	PTR	bw-in-f99.google.com.
                    
            """
            rdf = addr
            if isinstance(addr, str):
                if (addr.find("::") >= 0): #IPv6
                    rdf =  _ldns.ldns_rdf_new_frm_str(_ldns.LDNS_RDF_TYPE_AAAA, addr)
                else:
                    rdf =  _ldns.ldns_rdf_new_frm_str(_ldns.LDNS_RDF_TYPE_A, addr)
            return _ldns.ldns_get_rr_list_name_by_addr(self, rdf, aclass, flags)

        def print_to_file(self,output):
            """Print a resolver (in sofar that is possible) state to output."""
            _ldns.ldns_resolver_print(output,self)

        def axfr_start(self, domain, aclass):
            """Prepares the resolver for an axfr query. The query is sent and the answers can be read with axfr_next

               **Usage**
               ::
    
                  status = resolver.axfr_start("nic.cz", ldns.LDNS_RR_CLASS_IN)
                  if (status != ldns.LDNS_STATUS_OK): raise Exception("Can't start AXFR, error: %s" % ldns.ldns_get_errorstr_by_id(status))
                  #Print the results
                  while True:
                       rr = resolver.axfr_next()
                       if not rr: 
                          break

                       print rr

            """
            rdf = domain
            if isinstance(domain, str):
                rdf = _ldns.ldns_dname_new_frm_str(domain)
            return _ldns.ldns_axfr_start(self, rdf, aclass)
            #parameters: ldns_resolver *resolver, ldns_rdf *domain, ldns_rr_class c
            #retvals: int

        def axfr_complete(self):
            """returns true if the axfr transfer has completed (i.e. 2 SOA RRs and no errors were encountered)"""
            return _ldns.ldns_axfr_complete(self)
            #parameters: const ldns_resolver *,
            #retvals: bool

        def axfr_last_pkt(self):
            """returns a pointer to the last ldns_pkt that was sent by the server in the AXFR transfer uasable for instance to get the error code on failure"""
            return _ldns.ldns_axfr_last_pkt(self)
            #parameters: const ldns_resolver *,
            #retvals: ldns_pkt *

        def axfr_next(self):
            """get the next stream of RRs in a AXFR"""
            return _ldns.ldns_axfr_next(self)
            #parameters: ldns_resolver *,
            #retvals: ldns_rr *

            #LDNS_RESOLVER_METHODS_#
        def debug(self):
            """Get the debug status of the resolver.
               
               :returns: (bool) true if so, otherwise false
            """
            return _ldns.ldns_resolver_debug(self)
            #parameters: const ldns_resolver *,
            #retvals: bool

        def dec_nameserver_count(self):
            """Decrement the resolver's nameserver count.
            """
            _ldns.ldns_resolver_dec_nameserver_count(self)
            #parameters: ldns_resolver *,
            #retvals: 

        def defnames(self):
            return _ldns.ldns_resolver_defnames(self)
            #parameters: const ldns_resolver *,
            #retvals: bool

        def dnsrch(self):
            return _ldns.ldns_resolver_dnsrch(self)
            #parameters: const ldns_resolver *,
            #retvals: bool

        def dnssec(self):
            """Does the resolver do DNSSEC.
               
               :returns: (bool) true: yes, false: no
            """
            return _ldns.ldns_resolver_dnssec(self)
            #parameters: const ldns_resolver *,
            #retvals: bool

        def dnssec_anchors(self):
            """Get the resolver's DNSSEC anchors.
               
               :returns: (ldns_rr_list \*) an rr_list containg trusted DNSSEC anchors
            """
            return _ldns.ldns_resolver_dnssec_anchors(self)
            #parameters: const ldns_resolver *,
            #retvals: ldns_rr_list *

        def dnssec_cd(self):
            """Does the resolver set the CD bit.
               
               :returns: (bool) true: yes, false: no
            """
            return _ldns.ldns_resolver_dnssec_cd(self)
            #parameters: const ldns_resolver *,
            #retvals: bool

        def domain(self):
            """What is the default dname to add to relative queries.
               
               :returns: (ldns_rdf \*) the dname which is added
            """
            return _ldns.ldns_resolver_domain(self)
            #parameters: const ldns_resolver *,
            #retvals: ldns_rdf *

        def edns_udp_size(self):
            """Get the resolver's udp size.
               
               :returns: (uint16_t) the udp mesg size
            """
            return _ldns.ldns_resolver_edns_udp_size(self)
            #parameters: const ldns_resolver *,
            #retvals: uint16_t

        def fail(self):
            """Does the resolver only try the first nameserver.
               
               :returns: (bool) true: yes, fail, false: no, try the others
            """
            return _ldns.ldns_resolver_fail(self)
            #parameters: const ldns_resolver *,
            #retvals: bool

        def fallback(self):
            """Get the truncation fallback status.
               
               :returns: (bool) whether the truncation fallback mechanism is used
            """
            return _ldns.ldns_resolver_fallback(self)
            #parameters: const ldns_resolver *,
            #retvals: bool

        def igntc(self):
            """Does the resolver ignore the TC bit (truncated).
               
               :returns: (bool) true: yes, false: no
            """
            return _ldns.ldns_resolver_igntc(self)
            #parameters: const ldns_resolver *,
            #retvals: bool

        def incr_nameserver_count(self):
            """Incremental the resolver's nameserver count.
            """
            _ldns.ldns_resolver_incr_nameserver_count(self)
            #parameters: ldns_resolver *,
            #retvals: 

        def ip6(self):
            """Does the resolver use ip6 or ip4.
               
               :returns: (uint8_t) 0: both, 1: ip4, 2:ip6
            """
            return _ldns.ldns_resolver_ip6(self)
            #parameters: const ldns_resolver *,
            #retvals: uint8_t

        def nameserver_count(self):
            """How many nameserver are configured in the resolver.
               
               :returns: (size_t) number of nameservers
            """
            return _ldns.ldns_resolver_nameserver_count(self)
            #parameters: const ldns_resolver *,
            #retvals: size_t

        def nameserver_rtt(self,pos):
            """Return the used round trip time for a specific nameserver.
               
               :param pos:
                   the index to the nameserver
               :returns: (size_t) the rrt, 0: infinite, >0: undefined (as of * yet)
            """
            return _ldns.ldns_resolver_nameserver_rtt(self,pos)
            #parameters: const ldns_resolver *,size_t,
            #retvals: size_t

        def nameservers(self):
            """Return the configured nameserver ip address.
               
               :returns: (ldns_rdf \*\*) a ldns_rdf pointer to a list of the addresses
            """
            return _ldns.ldns_resolver_nameservers(self)
            #parameters: const ldns_resolver *,
            #retvals: ldns_rdf **

        def nameservers_randomize(self):
            """randomize the nameserver list in the resolver
            """
            _ldns.ldns_resolver_nameservers_randomize(self)
            #parameters: ldns_resolver *,
            #retvals: 

        def pop_nameserver(self):
            """pop the last nameserver from the resolver.
               
               :returns: (ldns_rdf \*) the popped address or NULL if empty
            """
            return _ldns.ldns_resolver_pop_nameserver(self)
            #parameters: ldns_resolver *,
            #retvals: ldns_rdf *

        def port(self):
            """Get the port the resolver should use.
               
               :returns: (uint16_t) the port number
            """
            return _ldns.ldns_resolver_port(self)
            #parameters: const ldns_resolver *,
            #retvals: uint16_t

        def prepare_query_pkt(self,name,t,c,f):
            """Form a query packet from a resolver and name/type/class combo.
               
               :param name:
               :param t:
                   query for this type (may be 0, defaults to A)
               :param c:
                   query for this class (may be 0, default to IN)
               :param f:
                   the query flags
               :returns: * (ldns_status) ldns_pkt* a packet with the reply from the nameserver
                         * (ldns_pkt \*\*) query packet class 
            """
            return _ldns.ldns_resolver_prepare_query_pkt(self,name,t,c,f)
            #parameters: ldns_resolver *,const ldns_rdf *,ldns_rr_type,ldns_rr_class,uint16_t,
            #retvals: ldns_status,ldns_pkt **

        def push_dnssec_anchor(self,rr):
            """Push a new trust anchor to the resolver.
               
               It must be a DS or DNSKEY rr
               
               :param rr:
                   the RR to add as a trust anchor.
               :returns: (ldns_status) a status
            """
            return _ldns.ldns_resolver_push_dnssec_anchor(self,rr)
            #parameters: ldns_resolver *,ldns_rr *,
            #retvals: ldns_status

        def push_nameserver(self,n):
            """push a new nameserver to the resolver.
               
               It must be an IP address v4 or v6.
               
               :param n:
                   the ip address
               :returns: (ldns_status) ldns_status a status
            """
            return _ldns.ldns_resolver_push_nameserver(self,n)
            #parameters: ldns_resolver *,ldns_rdf *,
            #retvals: ldns_status

        def push_nameserver_rr(self,rr):
            """push a new nameserver to the resolver.
               
               It must be an A or AAAA RR record type
               
               :param rr:
                   the resource record
               :returns: (ldns_status) ldns_status a status
            """
            return _ldns.ldns_resolver_push_nameserver_rr(self,rr)
            #parameters: ldns_resolver *,ldns_rr *,
            #retvals: ldns_status

        def push_nameserver_rr_list(self,rrlist):
            """push a new nameserver rr_list to the resolver.
               
               :param rrlist:
                   the rr_list to push
               :returns: (ldns_status) ldns_status a status
            """
            return _ldns.ldns_resolver_push_nameserver_rr_list(self,rrlist)
            #parameters: ldns_resolver *,ldns_rr_list *,
            #retvals: ldns_status

        def push_searchlist(self,rd):
            """Push a new rd to the resolver's searchlist.
               
               :param rd:
                   to push
            """
            _ldns.ldns_resolver_push_searchlist(self,rd)
            #parameters: ldns_resolver *,ldns_rdf *,
            #retvals: 

        def query(self,name,atype=_ldns.LDNS_RR_TYPE_A,aclass=_ldns.LDNS_RR_CLASS_IN,flags=_ldns.LDNS_RD):
            """Send a query to a nameserver.
               
               :param name: (ldns_rdf) the name to look for
               :param atype: the RR type to use
               :param aclass: the RR class to use
               :param flags: give some optional flags to the query
               :returns: (ldns_pkt) a packet with the reply from the nameserver if _defnames is true the default domain will be added
            """
            return _ldns.ldns_resolver_query(self,name,atype,aclass,flags)
            #parameters: const ldns_resolver *,const ldns_rdf *,ldns_rr_type,ldns_rr_class,uint16_t,
            #retvals: ldns_pkt *

        def random(self):
            """Does the resolver randomize the nameserver before usage.
               
               :returns: (bool) true: yes, false: no
            """
            return _ldns.ldns_resolver_random(self)
            #parameters: const ldns_resolver *,
            #retvals: bool

        def recursive(self):
            """Is the resolver set to recurse.
               
               :returns: (bool) true if so, otherwise false
            """
            return _ldns.ldns_resolver_recursive(self)
            #parameters: const ldns_resolver *,
            #retvals: bool

        def retrans(self):
            """Get the retransmit interval.
               
               :returns: (uint8_t) the retransmit interval
            """
            return _ldns.ldns_resolver_retrans(self)
            #parameters: const ldns_resolver *,
            #retvals: uint8_t

        def retry(self):
            """Get the number of retries.
               
               :returns: (uint8_t) the number of retries
            """
            return _ldns.ldns_resolver_retry(self)
            #parameters: const ldns_resolver *,
            #retvals: uint8_t

        def rtt(self):
            """Return the used round trip times for the nameservers.
               
               :returns: (size_t \*) a size_t* pointer to the list. yet)
            """
            return _ldns.ldns_resolver_rtt(self)
            #parameters: const ldns_resolver *,
            #retvals: size_t *

        def search(self,rdf,t,c,flags):
            """Send the query for using the resolver and take the search list into account The search algorithm is as follows: If the name is absolute, try it as-is, otherwise apply the search list.
               
               :param rdf:
               :param t:
                   query for this type (may be 0, defaults to A)
               :param c:
                   query for this class (may be 0, default to IN)
               :param flags:
                   the query flags
               :returns: (ldns_pkt \*) ldns_pkt* a packet with the reply from the nameserver
            """
            return _ldns.ldns_resolver_search(self,rdf,t,c,flags)
            #parameters: const ldns_resolver *,const ldns_rdf *,ldns_rr_type,ldns_rr_class,uint16_t,
            #retvals: ldns_pkt *

        def searchlist(self):
            """What is the searchlist as used by the resolver.
               
               :returns: (ldns_rdf \*\*) a ldns_rdf pointer to a list of the addresses
            """
            return _ldns.ldns_resolver_searchlist(self)
            #parameters: const ldns_resolver *,
            #retvals: ldns_rdf \*\*

        def searchlist_count(self):
            """Return the resolver's searchlist count.
               
               :returns: (size_t) the searchlist count
            """
            return _ldns.ldns_resolver_searchlist_count(self)
            #parameters: const ldns_resolver *,
            #retvals: size_t

        def send(self,name,t,c,flags):
            """Send the query for name as-is.
               
               :param name:
               :param t:
                   query for this type (may be 0, defaults to A)
               :param c:
                   query for this class (may be 0, default to IN)
               :param flags:
                   the query flags
               :returns: * (ldns_status) ldns_pkt* a packet with the reply from the nameserver
                         * (ldns_pkt \*\*) 
            """
            return _ldns.ldns_resolver_send(self,name,t,c,flags)
            #parameters: ldns_resolver *,const ldns_rdf *,ldns_rr_type,ldns_rr_class,uint16_t,
            #retvals: ldns_status,ldns_pkt **

        def send_pkt(self,query_pkt):
            """Send the given packet to a nameserver.
               
               :param query_pkt:
               :returns: * (ldns_status) 
                         * (ldns_pkt \*\*) 
            """
            return _ldns.ldns_resolver_send_pkt(self,query_pkt)
            #parameters: ldns_resolver *,ldns_pkt *,
            #retvals: ldns_status,ldns_pkt **

        def set_debug(self,b):
            """Set the resolver debugging.
               
               :param b:
                   true: debug on: false debug off
            """
            _ldns.ldns_resolver_set_debug(self,b)
            #parameters: ldns_resolver *,bool,
            #retvals: 

        def set_defnames(self,b):
            """Whether the resolver uses the name set with _set_domain.
               
               :param b:
                   true: use the defaults, false: don't use them
            """
            _ldns.ldns_resolver_set_defnames(self,b)
            #parameters: ldns_resolver *,bool,
            #retvals: 

        def set_dnsrch(self,b):
            """Whether the resolver uses the searchlist.
               
               :param b:
                   true: use the list, false: don't use the list
            """
            _ldns.ldns_resolver_set_dnsrch(self,b)
            #parameters: ldns_resolver *,bool,
            #retvals: 

        def set_dnssec(self,b):
            """Whether the resolver uses DNSSEC.
               
               :param b:
                   true: use DNSSEC, false: don't use DNSSEC
            """
            _ldns.ldns_resolver_set_dnssec(self,b)
            #parameters: ldns_resolver *,bool,
            #retvals: 

        def set_dnssec_anchors(self,l):
            """Set the resolver's DNSSEC anchor list directly.
               
               RRs should be of type DS or DNSKEY.
               
               :param l:
                   the list of RRs to use as trust anchors
            """
            _ldns.ldns_resolver_set_dnssec_anchors(self,l)
            #parameters: ldns_resolver *,ldns_rr_list *,
            #retvals: 

        def set_dnssec_cd(self,b):
            """Whether the resolver uses the checking disable bit.
               
               :param b:
                   true: enable , false: don't use TCP
            """
            _ldns.ldns_resolver_set_dnssec_cd(self,b)
            #parameters: ldns_resolver *,bool,
            #retvals: 

        def set_domain(self,rd):
            """Set the resolver's default domain.
               
               This gets appended when no absolute name is given
               
               :param rd:
                   the name to append
            """
            _ldns.ldns_resolver_set_domain(self,rd)
            #parameters: ldns_resolver *,ldns_rdf *,
            #retvals: 

        def set_edns_udp_size(self,s):
            """Set maximum udp size.
               
               :param s:
                   the udp max size
            """
            _ldns.ldns_resolver_set_edns_udp_size(self,s)
            #parameters: ldns_resolver *,uint16_t,
            #retvals: 

        def set_fail(self,b):
            """Whether or not to fail after one failed query.
               
               :param b:
                   true: yes fail, false: continue with next nameserver
            """
            _ldns.ldns_resolver_set_fail(self,b)
            #parameters: ldns_resolver *,bool,
            #retvals: 

        def set_fallback(self,fallback):
            """Set whether the resolvers truncation fallback mechanism is used when ldns_resolver_query() is called.
               
               :param fallback:
                   whether to use the fallback mechanism
            """
            _ldns.ldns_resolver_set_fallback(self,fallback)
            #parameters: ldns_resolver *,bool,
            #retvals: 

        def set_igntc(self,b):
            """Whether or not to ignore the TC bit.
               
               :param b:
                   true: yes ignore, false: don't ignore
            """
            _ldns.ldns_resolver_set_igntc(self,b)
            #parameters: ldns_resolver *,bool,
            #retvals: 

        def set_ip6(self,i):
            """Whether the resolver uses ip6.
               
               :param i:
                   0: no pref, 1: ip4, 2: ip6
            """
            _ldns.ldns_resolver_set_ip6(self,i)
            #parameters: ldns_resolver *,uint8_t,
            #retvals: 

        def set_nameserver_count(self,c):
            """Set the resolver's nameserver count directly.
               
               :param c:
                   the nameserver count
            """
            _ldns.ldns_resolver_set_nameserver_count(self,c)
            #parameters: ldns_resolver *,size_t,
            #retvals: 

        def set_nameserver_rtt(self,pos,value):
            """Set round trip time for a specific nameserver.
               
               Note this currently differentiates between: unreachable and reachable.
               
               :param pos:
                   the nameserver position
               :param value:
                   the rtt
            """
            _ldns.ldns_resolver_set_nameserver_rtt(self,pos,value)
            #parameters: ldns_resolver *,size_t,size_t,
            #retvals: 

        def set_nameservers(self,rd):
            """Set the resolver's nameserver count directly by using an rdf list.
               
               :param rd:
                   the resolver addresses
            """
            _ldns.ldns_resolver_set_nameservers(self,rd)
            #parameters: ldns_resolver *,ldns_rdf **,
            #retvals: 

        def set_port(self,p):
            """Set the port the resolver should use.
               
               :param p:
                   the port number
            """
            _ldns.ldns_resolver_set_port(self,p)
            #parameters: ldns_resolver *,uint16_t,
            #retvals: 

        def set_random(self,b):
            """Should the nameserver list be randomized before each use.
               
               :param b:
                   true: randomize, false: don't
            """
            _ldns.ldns_resolver_set_random(self,b)
            #parameters: ldns_resolver *,bool,
            #retvals: 

        def set_recursive(self,b):
            """Set the resolver recursion.
               
               :param b:
                   true: set to recurse, false: unset
            """
            _ldns.ldns_resolver_set_recursive(self,b)
            #parameters: ldns_resolver *,bool,
            #retvals: 

        def set_retrans(self,re):
            """Set the resolver retrans timeout (in seconds).
               
               :param re:
                   the retransmission interval in seconds
            """
            _ldns.ldns_resolver_set_retrans(self,re)
            #parameters: ldns_resolver *,uint8_t,
            #retvals: 

        def set_retry(self,re):
            """Set the resolver retry interval (in seconds).
               
               :param re:
                   the retry interval
            """
            _ldns.ldns_resolver_set_retry(self,re)
            #parameters: ldns_resolver *,uint8_t,
            #retvals: 

        def set_rtt(self,rtt):
            """Set round trip time for all nameservers.
               
               Note this currently differentiates between: unreachable and reachable.
               
               :param rtt:
                   a list with the times
            """
            _ldns.ldns_resolver_set_rtt(self,rtt)
            #parameters: ldns_resolver *,size_t *,
            #retvals: 

        def set_searchlist_count(self,c):
            _ldns.ldns_resolver_set_searchlist_count(self,c)
            #parameters: ldns_resolver *,size_t,
            #retvals: 

        def set_timeout(self,timeout):
            """Set the resolver's socket time out when talking to remote hosts.
               
               :param timeout:
                   the timeout to use
            """
            _ldns.ldns_resolver_set_timeout(self,timeout)
            #parameters: ldns_resolver *,struct timeval,
            #retvals: 

        def set_tsig_algorithm(self,tsig_algorithm):
            """Set the tsig algorithm.
               
               :param tsig_algorithm:
                   the tsig algorithm
            """
            _ldns.ldns_resolver_set_tsig_algorithm(self,tsig_algorithm)
            #parameters: ldns_resolver *,char *,
            #retvals: 

        def set_tsig_keydata(self,tsig_keydata):
            """Set the tsig key data.
               
               :param tsig_keydata:
                   the key data
            """
            _ldns.ldns_resolver_set_tsig_keydata(self,tsig_keydata)
            #parameters: ldns_resolver *,char *,
            #retvals: 

        def set_tsig_keyname(self,tsig_keyname):
            """Set the tsig key name.
               
               :param tsig_keyname:
                   the tsig key name
            """
            _ldns.ldns_resolver_set_tsig_keyname(self,tsig_keyname)
            #parameters: ldns_resolver *,char *,
            #retvals: 

        def set_usevc(self,b):
            """Whether the resolver uses a virtual circuit (TCP).
               
               :param b:
                   true: use TCP, false: don't use TCP
            """
            _ldns.ldns_resolver_set_usevc(self,b)
            #parameters: ldns_resolver *,bool,
            #retvals: 

        def timeout(self):
            """What is the timeout on socket connections.
               
               :returns: (struct timeval) the timeout as struct timeval
            """
            return _ldns.ldns_resolver_timeout(self)
            #parameters: const ldns_resolver *,
            #retvals: struct timeval

        def trusted_key(self,keys,trusted_keys):
            """Returns true if at least one of the provided keys is a trust anchor.
               
               :param keys:
                   the keyset to check
               :param trusted_keys:
                   the subset of trusted keys in the 'keys' rrset
               :returns: (bool) true if at least one of the provided keys is a configured trust anchor
            """
            return _ldns.ldns_resolver_trusted_key(self,keys,trusted_keys)
            #parameters: const ldns_resolver *,ldns_rr_list *,ldns_rr_list *,
            #retvals: bool

        def tsig_algorithm(self):
            """Return the tsig algorithm as used by the nameserver.
               
               :returns: (char \*) the algorithm used.
            """
            return _ldns.ldns_resolver_tsig_algorithm(self)
            #parameters: const ldns_resolver *,
            #retvals: char *

        def tsig_keydata(self):
            """Return the tsig keydata as used by the nameserver.
               
               :returns: (char \*) the keydata used.
            """
            return _ldns.ldns_resolver_tsig_keydata(self)
            #parameters: const ldns_resolver *,
            #retvals: char *

        def tsig_keyname(self):
            """Return the tsig keyname as used by the nameserver.
               
               :returns: (char \*) the name used.
            """
            return _ldns.ldns_resolver_tsig_keyname(self)
            #parameters: const ldns_resolver *,
            #retvals: char *

        def usevc(self):
            """Does the resolver use tcp or udp.
               
               :returns: (bool) true: tcp, false: udp
            """
            return _ldns.ldns_resolver_usevc(self)
            #parameters: const ldns_resolver *,
            #retvals: bool

            #_LDNS_RESOLVER_METHODS#
 %}
} 
