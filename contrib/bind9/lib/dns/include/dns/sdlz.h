/*
 * Portions Copyright (C) 2005-2007, 2009-2011  Internet Systems Consortium, Inc. ("ISC")
 * Portions Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Copyright (C) 2002 Stichting NLnet, Netherlands, stichting@nlnet.nl.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND STICHTING NLNET
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * STICHTING NLNET BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * The development of Dynamically Loadable Zones (DLZ) for Bind 9 was
 * conceived and contributed by Rob Butler.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ROB BUTLER
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * ROB BUTLER BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS
 * OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE
 * USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $Id: sdlz.h,v 1.14.8.2 2011-03-17 23:47:06 tbox Exp $ */

/*! \file dns/sdlz.h */

#ifndef SDLZ_H
#define SDLZ_H 1

#include <dns/dlz.h>

ISC_LANG_BEGINDECLS

#define DNS_SDLZFLAG_THREADSAFE		0x00000001U
#define DNS_SDLZFLAG_RELATIVEOWNER	0x00000002U
#define DNS_SDLZFLAG_RELATIVERDATA	0x00000004U

 /* A simple DLZ database. */
typedef struct dns_sdlz_db dns_sdlz_db_t;

 /* A simple DLZ database lookup in progress. */
typedef struct dns_sdlzlookup dns_sdlzlookup_t;

 /* A simple DLZ database traversal in progress. */
typedef struct dns_sdlzallnodes dns_sdlzallnodes_t;

typedef isc_result_t (*dns_sdlzallnodesfunc_t)(const char *zone,
					       void *driverarg,
					       void *dbdata,
					       dns_sdlzallnodes_t *allnodes);
/*%<
 * Method prototype.  Drivers implementing the SDLZ interface may
 * supply an all nodes method.  This method is called when the DNS
 * server is performing a zone transfer query, after the allow zone
 * transfer method has been called.  This method is only called if the
 * allow zone transfer method returned ISC_R_SUCCESS.  This method and
 * the allow zone transfer method are both required for zone transfers
 * to be supported.  If the driver generates data dynamically (instead
 * of searching in a database for it) it should not implement this
 * function as a zone transfer would be meaningless.  A SDLZ driver
 * does not have to implement an all nodes method.
 */

typedef isc_result_t (*dns_sdlzallowzonexfr_t)(void *driverarg,
					       void *dbdata, const char *name,
					       const char *client);

/*%<
 * Method prototype.  Drivers implementing the SDLZ interface may
 * supply an allow zone transfer method.  This method is called when
 * the DNS server is performing a zone transfer query, before the all
 * nodes method can be called.  This method and the all node method
 * are both required for zone transfers to be supported.  If the
 * driver generates data dynamically (instead of searching in a
 * database for it) it should not implement this function as a zone
 * transfer would be meaningless.  A SDLZ driver does not have to
 * implement an allow zone transfer method.
 *
 * This method should return ISC_R_SUCCESS if the zone is supported by
 * the database and a zone transfer is allowed for the specified
 * client.  If the zone is supported by the database, but zone
 * transfers are not allowed for the specified client this method
 * should return ISC_R_NOPERM..  Lastly the method should return
 * ISC_R_NOTFOUND if the zone is not supported by the database.  If an
 * error occurs it should return a result code indicating the type of
 * error.
 */

typedef isc_result_t (*dns_sdlzauthorityfunc_t)(const char *zone,
						void *driverarg, void *dbdata,
						dns_sdlzlookup_t *lookup);

/*%<
 * Method prototype.  Drivers implementing the SDLZ interface may
 * supply an authority method.  This method is called when the DNS
 * server is performing a query, after both the find zone and lookup
 * methods have been called.  This method is required if the lookup
 * function does not supply authority information for the dns
 * record. A SDLZ driver does not have to implement an authority
 * method.
 */

typedef isc_result_t (*dns_sdlzcreate_t)(const char *dlzname,
					 unsigned int argc, char *argv[],
					 void *driverarg, void **dbdata);

/*%<
 * Method prototype.  Drivers implementing the SDLZ interface may
 * supply a create method.  This method is called when the DNS server
 * is starting up and creating drivers for use later. A SDLZ driver
 * does not have to implement a create method.
 */

typedef void (*dns_sdlzdestroy_t)(void *driverarg, void *dbdata);

/*%<
 * Method prototype.  Drivers implementing the SDLZ interface may
 * supply a destroy method.  This method is called when the DNS server
 * is shutting down and no longer needs the driver.  A SDLZ driver does
 * not have to implement a destroy method.
 */

typedef isc_result_t
(*dns_sdlzfindzone_t)(void *driverarg, void *dbdata, const char *name);

/*%<
 * Method prototype.  Drivers implementing the SDLZ interface MUST
 * supply a find zone method.  This method is called when the DNS
 * server is performing a query to to determine if 'name' is a
 * supported dns zone.  The find zone method will be called with the
 * longest possible name first, and continue to be called with
 * successively shorter domain names, until any of the following
 * occur:
 *
 * \li	1) the function returns (ISC_R_SUCCESS) indicating a zone name
 *	   match.
 *
 * \li	2) a problem occurs, and the functions returns anything other than
 *	   (ISC_R_NOTFOUND)
 *
 * \li	3) we run out of domain name labels. I.E. we have tried the
 *	   shortest domain name
 *
 * \li	4) the number of labels in the domain name is less than min_labels
 *	   for dns_dlzfindzone
 *
 * The driver's find zone method should return ISC_R_SUCCESS if the
 * zone is supported by the database.  Otherwise it should return
 * ISC_R_NOTFOUND, if the zone is not supported.  If an error occurs
 * it should return a result code indicating the type of error.
 */

typedef isc_result_t
(*dns_sdlzlookupfunc_t)(const char *zone, const char *name, void *driverarg,
			void *dbdata, dns_sdlzlookup_t *lookup);

/*%<
 * Method prototype.  Drivers implementing the SDLZ interface MUST
 * supply a lookup method.  This method is called when the DNS server
 * is performing a query, after the find zone and before any other
 * methods have been called.  This function returns record DNS record
 * information using the dns_sdlz_putrr and dns_sdlz_putsoa functions.
 * If this function supplies authority information for the DNS record
 * the authority method is not required.  If it does not, the
 * authority function is required.  A SDLZ driver must implement a
 * lookup method.
 */

typedef isc_result_t (*dns_sdlznewversion_t)(const char *zone,
					     void *driverarg, void *dbdata,
					     void **versionp);
/*%<
 * Method prototype.  Drivers implementing the SDLZ interface may
 * supply a newversion method.  This method is called to start a
 * write transaction on a zone and should only be implemented by
 * writeable backends.
 * When implemented, the driver should create a new transaction, and
 * fill *versionp with a pointer to the transaction state. The
 * closeversion function will be called to close the transaction.
 */

typedef void (*dns_sdlzcloseversion_t)(const char *zone, isc_boolean_t commit,
				       void *driverarg, void *dbdata,
				       void **versionp);
/*%<
 * Method prototype.  Drivers implementing the SDLZ interface must
 * supply a closeversion method if they supply a newversion method.
 * When implemented, the driver should close the given transaction,
 * committing changes if 'commit' is ISC_TRUE. If 'commit' is not true
 * then all changes should be discarded and the database rolled back.
 * If the call is successful then *versionp should be set to NULL
 */

typedef isc_result_t (*dns_sdlzconfigure_t)(dns_view_t *view, void *driverarg,
					    void *dbdata);
/*%<
 * Method prototype.  Drivers implementing the SDLZ interface may
 * supply a configure method. When supplied, it will be called
 * immediately after the create method to give the driver a chance
 * to configure writeable zones
 */


typedef isc_boolean_t (*dns_sdlzssumatch_t)(const char *signer,
					    const char *name,
					    const char *tcpaddr,
					    const char *type,
					    const char *key,
					    isc_uint32_t keydatalen,
					    unsigned char *keydata,
					    void *driverarg,
					    void *dbdata);

/*%<
 * Method prototype.  Drivers implementing the SDLZ interface may
 * supply a ssumatch method. If supplied, then ssumatch will be
 * called to authorize any zone updates. The driver should return
 * ISC_TRUE to allow the update, and ISC_FALSE to deny it. For a DLZ
 * controlled zone, this is the only access control on updates.
 */


typedef isc_result_t (*dns_sdlzmodrdataset_t)(const char *name,
					      const char *rdatastr,
					      void *driverarg, void *dbdata,
					      void *version);
/*%<
 * Method prototype.  Drivers implementing the SDLZ interface may
 * supply addrdataset and subtractrdataset methods. If supplied, then these
 * will be called when rdatasets are added/subtracted during
 * updates. The version parameter comes from a call to the sdlz
 * newversion() method from the driver. The rdataset parameter is a
 * linearise string representation of the rdataset change. The format
 * is the same as used by dig when displaying records. The fields are
 * tab delimited.
 */

typedef isc_result_t (*dns_sdlzdelrdataset_t)(const char *name,
					      const char *type,
					      void *driverarg, void *dbdata,
					      void *version);
/*%<
 * Method prototype.  Drivers implementing the SDLZ interface may
 * supply a delrdataset method. If supplied, then this
 * function will be called when rdatasets are deleted during
 * updates. The call should remove all rdatasets of the given type for
 * the specified name.
 */

typedef struct dns_sdlzmethods {
	dns_sdlzcreate_t	create;
	dns_sdlzdestroy_t	destroy;
	dns_sdlzfindzone_t	findzone;
	dns_sdlzlookupfunc_t	lookup;
	dns_sdlzauthorityfunc_t	authority;
	dns_sdlzallnodesfunc_t	allnodes;
	dns_sdlzallowzonexfr_t	allowzonexfr;
	dns_sdlznewversion_t    newversion;
	dns_sdlzcloseversion_t  closeversion;
	dns_sdlzconfigure_t	configure;
	dns_sdlzssumatch_t	ssumatch;
	dns_sdlzmodrdataset_t	addrdataset;
	dns_sdlzmodrdataset_t	subtractrdataset;
	dns_sdlzdelrdataset_t	delrdataset;
} dns_sdlzmethods_t;

isc_result_t
dns_sdlzregister(const char *drivername, const dns_sdlzmethods_t *methods,
		 void *driverarg, unsigned int flags, isc_mem_t *mctx,
		 dns_sdlzimplementation_t **sdlzimp);
/*%<
 * Register a dynamically loadable zones (dlz) driver for the database
 * type 'drivername', implemented by the functions in '*methods'.
 *
 * sdlzimp must point to a NULL dns_sdlzimplementation_t pointer.
 * That is, sdlzimp != NULL && *sdlzimp == NULL.  It will be assigned
 * a value that will later be used to identify the driver when
 * deregistering it.
 */

void
dns_sdlzunregister(dns_sdlzimplementation_t **sdlzimp);

/*%<
 * Removes the sdlz driver from the list of registered sdlz drivers.
 * There must be no active sdlz drivers of this type when this
 * function is called.
 */

typedef isc_result_t dns_sdlz_putnamedrr_t(dns_sdlzallnodes_t *allnodes,
					   const char *name,
					   const char *type,
					   dns_ttl_t ttl,
					   const char *data);
dns_sdlz_putnamedrr_t dns_sdlz_putnamedrr;

/*%<
 * Add a single resource record to the allnodes structure to be later
 * parsed into a zone transfer response.
 */

typedef isc_result_t dns_sdlz_putrr_t(dns_sdlzlookup_t *lookup,
				      const char *type,
				      dns_ttl_t ttl,
				      const char *data);
dns_sdlz_putrr_t dns_sdlz_putrr;
/*%<
 * Add a single resource record to the lookup structure to be later
 * parsed into a query response.
 */

typedef isc_result_t dns_sdlz_putsoa_t(dns_sdlzlookup_t *lookup,
				       const char *mname,
				       const char *rname,
				       isc_uint32_t serial);
dns_sdlz_putsoa_t dns_sdlz_putsoa;
/*%<
 * This function may optionally be called from the 'authority'
 * callback to simplify construction of the SOA record for 'zone'.  It
 * will provide a SOA listing 'mname' as as the master server and
 * 'rname' as the responsible person mailbox.  It is the
 * responsibility of the driver to increment the serial number between
 * responses if necessary.  All other SOA fields will have reasonable
 * default values.
 */


typedef isc_result_t dns_sdlz_setdb_t(dns_dlzdb_t *dlzdatabase,
				      dns_rdataclass_t rdclass,
				      dns_name_t *name,
				      dns_db_t **dbp);
dns_sdlz_setdb_t dns_sdlz_setdb;
/*%<
 * Create the database pointers for a writeable SDLZ zone
 */


ISC_LANG_ENDDECLS

#endif /* SDLZ_H */
