/*
 * Fundamental constants relating to ethernet.
 *
 * $Id$
 *
 */

#define	ETHER_ADDR_LEN		6
#define ETHER_TYPE_LEN		2
#define	ETHER_CRC_LENGTH	4
#define	ETHER_HDR_SIZE		(ETHER_ADDR_LEN*2+ETHER_TYPE_LEN)
#define ETHER_MIN_LEN		64
#define ETHER_MAX_LEN		1518
