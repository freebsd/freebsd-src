.NC "The Design of the ARGO Network Layer"
.sh 1 "End System to Intermediate System Routing Protocol"
.pp
The following sections describe the design of the End System to Intermediate
System (ES-IS) Routing Exchange Protocol.
.sh 2 "Overview"
.nf
- protocol involves sending/receiving hello pdus.
- timers determine
	- when to send information
	- when to discard information
- want to keep as much of the work outside of kernel
- only put functions and tables in kernel when necessary
.sh 2 "Supported Features (brief overview of each)"
- report configuration (both ES and IS)
- record configuration (both ES and IS)
- flush configuration (both ES and IS)
- query configuration (ES only)
- configuration response (ES only)
- request redirect (IS only)
- record redirect (ES only)
- flush old redirect (ES only)
- multicast vs. broadcast (using broadcast only)
.sh 2 "Kernel Resident Features"
.sh 3 "Support for PDU Transmission"
- need mechanism to send/receive PDUs
- utilize ES-IS socket (like raw socket)
- socket(AF_ISO, SOCK_DGRAM, ISOPROTO_ESIS)
.sh 4 "Sending PDUs"
- sendmsg() used for transmitting PDUS
- data will be pre-formed ES-IS PDU
- no checks will be made on the pdu format
- addr_snpa is the destination (to)
- before sending, socket must be associated with a particular interface
	this is done via setsockopt():
		ESISOPT_SETIF - option
		buffer is name of interface, ie. "un0"
.sh 4 "Receiving PDUs"
- recvmsg() used for receiving PDUs
- data will be:
	#define	ESIS_PDU
	#define ESIS_CONFIG_RESP
	#define ESIS_REDIR_REQ
	struct esis_indication {
		short	ei_type;		/* type of indication */
		union {
			struct ? config_resp
			struct ? redir_req
			char	 pdu[0]
		} ei_u;
	}
- no checks will be made on the pdu format
- addr_snpa is the source (from)
.sh 4 "Addressing"
- ES-IS PDUs are sent to SNPA.
- addresses used are SN addresses, not NSAP addresses
- format of msg_name (part of msghdr) struct sockaddr_iso
	afi = 0	/* means special snpa address */
	isoa_u is struct addr_snpa
		struct addr_snpa {
			char	sn_addr[7];	/* snpa addr */
		}
	isoa_len is number of bytes of sn_addr that are valid

- sn_addr may be a unicast or multicast address
- multicast addresses will be faked via broadcast addresses
.sh 3 "NSAP to SNPA translation"
- translation from NSAP to SNAP required for every CLNP PDU sent
- function provided by iso_nsap_to_snpa

	iso_nsap_to_snpa(ifp, m, nsap, snpa)
	struct ifnet		*ifp;	/* outgoing interface */
	struct mbuf			*m;		/* pkt */
	struct sockaddr_iso	*nsap;	/* destination address */
	char				*snpa;	/* RESULT: snpa address */
	{
		if (nsap.afi == AFI_SNPAADDR) {
			copy snpa addr from nsap into snpa
			return SUCCESS
		} else {
			scan RIB for (RIB.nsap == nsap)
			if (found) {
				copy RIB.snpa into snpa
				return SUCCESS
			}
			scan RIB for (RIB.type == IS) && (RIB.ifp = ifp)
			if (found) {
				copy RIB.snpa into snpa
				return SUCCESS
			}
			if (ifp allows multicast) {
				/* invoke query configuration function */
				copy ifp.ifaddr.ifa_all_es into snpa
				return SUCCESS
			}

			return FAILURE
		}
	}

- NSAP to SNPA table resides in kernel so CLNP has quick access
- entries added/timed-out of table via user level ES-IS daemon
.sh 3 "Query Configuration Functon"
- invoked when iso_nsap_to_snpa
	- requires snpa, but
	- does not find a match for dest nsap, and
	- does not find a match for Any IS.
- clnp packet is sent to address "all ES" as specified in ifnet structure
	(for now, this is just the broadcast address)
.sh 3 "Configuration Response Function"
- invoked by clnp_input(), after determining that the packet received is
	destined for one of its NSAPs
- checks if sn_dst == "all ES" (for now, this is all hex ffs)
- if true, a copy of packet is made, and passed up to esis_input()
