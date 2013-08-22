/**
 * str2host.h - conversion from str to the host fmt
 *
 * a Net::DNS like library for C
 *
 * (c) NLnet Labs, 2005-2006
 *
 * See the file LICENSE for the license
 */

#ifndef LDNS_2HOST_H
#define LDNS_2HOST_H

#include <ldns/common.h>
#include <ldns/error.h>
#include <ldns/rr.h>
#include <ldns/rdata.h>
#include <ldns/packet.h>
#include <ldns/buffer.h>
#include <ctype.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \file
 *
 * Defines functions to convert dns data in presentation format or text files
 * to internal structures.
 */

/**
 * convert a byte into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] bytestr the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_int8(ldns_rdf **rd, const char *bytestr);

/**
 * convert a string to a int16 in wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] shortstr the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_int16(ldns_rdf **rd, const char *shortstr);

/**
 * convert a strings into a 4 byte int in wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] longstr the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_int32(ldns_rdf **rd, const char *longstr);

/**
 * convert a time string to a time value in wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] time the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_time(ldns_rdf **rd, const char *time);

/* convert string with NSEC3 salt to wireformat) 
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * return ldns_status
 */
ldns_status ldns_str2rdf_nsec3_salt(ldns_rdf **rd, const char *nsec3_salt);

/* convert a time period (think TTL's) to wireformat) 
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * return ldns_status
 */
ldns_status ldns_str2rdf_period(ldns_rdf **rd, const char *str);

/**
 * convert str with an A record into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_a(ldns_rdf **rd, const char *str);

/**
 * convert the str with an AAAA record into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_aaaa(ldns_rdf **rd, const char *str);

/**
 * convert a string into wireformat (think txt record)
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted (NULL terminated)
 * \return ldns_status
 */
ldns_status ldns_str2rdf_str(ldns_rdf **rd, const char *str);

/**
 * convert str with the apl record into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_apl(ldns_rdf **rd, const char *str);

/**
 * convert the string with the b64 data into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_b64(ldns_rdf **rd, const char *str);

/**
 * convert the string with the b32 ext hex data into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_b32_ext(ldns_rdf **rd, const char *str);

/**
 * convert a hex value into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_hex(ldns_rdf **rd, const char *str);

/**
 * convert string with nsec into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_nsec(ldns_rdf **rd, const char *str);

/**
 * convert a rrtype into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_type(ldns_rdf **rd, const char *str);

/**
 * convert string with a classname into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_class(ldns_rdf **rd, const char *str);

/**
 * convert an certificate algorithm value into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_cert_alg(ldns_rdf **rd, const char *str);

/**
 * convert and algorithm value into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_alg(ldns_rdf **rd, const char *str);

/**
 * convert a string with a unknown RR into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_unknown(ldns_rdf **rd, const char *str);

/**
 * convert string with a tsig? RR into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_tsig(ldns_rdf **rd, const char *str);

/**
 * convert string with a protocol service into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_service(ldns_rdf **rd, const char *str);

/**
 * convert a string with a LOC RR into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_loc(ldns_rdf **rd, const char *str);

/**
 * convert string with a WKS RR into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_wks(ldns_rdf **rd, const char *str);

/**
 * convert a str with a NSAP RR into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_nsap(ldns_rdf **rd, const char *str);

/**
 * convert a str with a ATMA RR into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_atma(ldns_rdf **rd, const char *str);

/**
 * convert a str with a IPSECKEY RR into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_ipseckey(ldns_rdf **rd, const char *str);

/**
 * convert a dname string into wireformat
 * \param[in] rd the rdf where to put the data
 * \param[in] str the string to be converted
 * \return ldns_status
 */
ldns_status ldns_str2rdf_dname(ldns_rdf **rd, const char *str);

#ifdef __cplusplus
}
#endif

#endif /* LDNS_2HOST_H */
