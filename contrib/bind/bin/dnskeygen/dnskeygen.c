#if !defined(lint) && !defined(SABER)
static const char rcsid[] = "$Id: dnskeygen.c,v 1.11.2.1 2001/04/26 02:56:06 marka Exp $";
#endif /* not lint */

/*
 * Portions Copyright (c) 1995-1999 by TISLabs at Network Associates, Inc.
 *
 * Permission to use, copy modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND NETWORK ASSOCIATES
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS.  IN NO EVENT SHALL
 * TRUSTED INFORMATION SYSTEMS BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THE SOFTWARE.
 */

#include "port_before.h"

#include <stdio.h>
#include <ctype.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include "arpa/nameser.h"

#include <isc/dst.h>

#include "port_after.h"

#define PRINT_SUPPORTED 2
#ifndef PATH_SEP
#define PATH_SEP '/'
#endif

static void usage(char *str, int full);

static short dsa_sizes[] = {512, 576, 640, 704, 768, 832, 896, 960, 1024, 0};
static char *prog;

int
main(int argc, char **argv) {
	DST_KEY *pubkey;
	char	*name=NULL;
	int      ch;
	char	 str[128];
	int	 alg = 0;
	int	 zone_key = 0, user_key = 0, end_key = 0, key_type = 0;
	int	 size = -1, exp = 0;
	int	 no_auth = 0, no_conf = 0;
	int	 sign_val = 0, flags = 0, protocol = -1;
	int      i, err = 0;
	extern char *optarg;

	dst_init();
	if ((prog = strrchr(argv[0], PATH_SEP)) == NULL)
		prog = strdup(argv[0]);
	else
		prog = strdup(++prog);

/* process input arguments */
	while ((ch = getopt(argc, argv, "achiuzn:s:p:D:H:R:F"))!= -1) {
	    switch (ch) {
		case 'a':
			no_auth = NS_KEY_NO_AUTH;
			break;
		case 'c':
			no_conf = NS_KEY_NO_CONF;
			break;
		case 'F':
			exp=1;
			break;
		case 'n':
			if (optarg)
				name = strdup(optarg);
			else
				usage("-n not followed by name", 0);
			i = strlen(name);
			if (name[i-1] != '.') {
				printf("** Adding dot to the name to make it"
				       " fully qualified domain name**\n");
				free(name);
				name = malloc(i+2);
				strcpy(name, optarg);
				strcat(name, ".");
			}
			break;
		case 'p':
			if (optarg && isdigit(optarg[0]))
				protocol = atoi(optarg);
			else
				usage("-p flag not followed by a number", 0);
			break;
		case 's':
			/* Default: not signatory key */
			if (optarg && isdigit(optarg[0]))
				sign_val = (int) atoi(optarg);
			else
				usage("-s flag requires a value",0);
			break;
		case 'h':
			end_key = NS_KEY_NAME_ENTITY;
			key_type++;
			break;
		case 'u' :
			user_key = NS_KEY_NAME_USER;
			key_type++;
			break ;
		case 'z':
			zone_key = NS_KEY_NAME_ZONE;
			key_type++;
			break;
		case 'H':
			if (optarg && isdigit(optarg[0]))
				size = (int) atoi(optarg);
			else
				usage("-H flag requires a size",0);
			if (alg != 0) 
				usage("Only ONE alg can be specified", 1);
			alg = KEY_HMAC_MD5;
			if (!dst_check_algorithm(alg)) 
				usage("Algorithm HMAC-MD5 not available", 
				      PRINT_SUPPORTED);
			break;
		case 'R':
			if (optarg && isdigit(optarg[0]))
				size = (int) atoi(optarg);
			else
				usage("-R flag requires a size",0);
			if (alg != 0) 
				usage("Only ONE alg can be specified", 1);
			alg = NS_ALG_MD5RSA;
			if (!dst_check_algorithm(alg)) 
				usage("Algorithm RSA not available", 
				      PRINT_SUPPORTED);
			break;
		case 'D':
			if (optarg && isdigit(optarg[0]))
				size = (int) atoi(optarg);
			else
				usage("-D flag requires a size", 0);
			if (alg != 0) 
				usage("Only ONE alg can be specified", 1);
			alg = NS_ALG_DSS;
			if (dst_check_algorithm(alg) == 0) 
				usage("Algorithm DSS not available",
				      PRINT_SUPPORTED);
			break;
		default:
		       err++;
		} /* switch */
	}	/* while (getopt) */

	/*
	 * Command line parsed make sure required parameters are present
	 */
	if (name == NULL)
		usage("No key name specified -n <name>", 1);

	if (alg == 0)
		usage("No algorithm specififed -{DHR}", 1);

	if (key_type == 0)
		usage("Key type -{zhu} must be specified", 1);
	else if (key_type > 1)
		usage("Only one key type -{zhu} must be specified", 1);

	if (alg == NS_ALG_DSS)
		no_conf = NS_KEY_NO_CONF; /* dss keys can not encrypt */

	if (protocol == -1) {
		if (zone_key || end_key)
			protocol = NS_KEY_PROT_DNSSEC;
		else
			protocol = NS_KEY_PROT_EMAIL;
	}
	if (protocol < 0 || protocol > 255)
		usage("Protocol value out of range [0..255]", 0);

	if (sign_val < 0 || sign_val > 15) {
		sprintf(str, "%s: Signatory value %d out of range[0..15]\n",
			prog, sign_val);
		usage(str, 0);
	}
	/* if any of bits 321 is set bit 0 can not be set*/
	if (sign_val & 0xe)
		sign_val &= 0xe;

	/* if a zone key make sure at least one of the signer flags is set  */
	if ((protocol == NS_KEY_PROT_DNSSEC) && (sign_val == 0))
		sign_val = 0x01;

	if (no_auth && no_conf) { /* null key specified */
		if (sign_val > 0)
			sign_val = 0x0; /* null key can not sign */
		if (size > 0)
			size = 0;       /* null key must have size 0 */
	}

	if (size > 0) {
		if (alg == NS_ALG_MD5RSA){
			if (size < 512 || size > 4096)
				usage("Size out of range", 1);
		}
		else if (exp)
			usage("-F can only be specified with -R", 0);
		if (alg == NS_ALG_DSS) {
			for (i = 0; dsa_sizes[i]; i++)
				if (size <= dsa_sizes[i])
					break;
			if (size != dsa_sizes[i])
				usage("Invalid DSS key size", 1);
		}
	}
	else if (size < 0)
		usage("No size specified", 0);
	else /* size == 0 */
		sign_val = 0;

	if (err)
		usage("errors encountered/unknown flag", 1);

	flags = no_conf | no_auth | end_key | user_key | zone_key | sign_val;

/* process defaults */
#ifdef WARN_NONZONE_SIGNER
	if (signer && (user_key | end_key))
		printf("Warning: User/End  key is allowed to sign\n");
#endif

	/* create a public/private key pair */
	if (alg == NS_ALG_MD5RSA)
		printf("Generating %d bit RSA Key for %s\n\n",size, name);
	else if (alg == NS_ALG_DSS)
		printf("Generating %d bit DSS Key for %s\n\n",size, name);
	else if (alg == KEY_HMAC_MD5) 
		printf("Generating %d bit HMAC-MD5 Key for %s\n\n",
		       size, name);

	/* Make the key
	 * dst_generate_key_pair will place result in files that it
	 * knows about K<name><foot>.public and K<name><foot>.private
	 */
	pubkey = dst_generate_key(name, size, exp, flags, protocol, alg);

	if (pubkey == NULL) {
		printf("Failed generating key for %s\n", name);
		exit(12);
	}

	if (dst_write_key(pubkey, DST_PRIVATE) < 0) {
		printf ("Failed to write private key for %s %d %d\n",
			name, pubkey->dk_id, pubkey->dk_alg);
		exit(12); 
	}

	if (dst_write_key(pubkey, DST_PUBLIC) <= 0) {
		if (access(name, F_OK))
			printf("Not allowed to overwrite existing file\n");
		else
			printf("Failed to write public key for %s %d %d\n",
			       name, pubkey->dk_id, pubkey->dk_alg);
		exit(12);
	}

	printf("Generated %d bit Key for %s id=%d alg=%d flags=%d\n\n",
	       size, name, pubkey->dk_id, pubkey->dk_alg,
	       pubkey->dk_flags);
	exit(0);
}

static void
usage(char *str, int flag){
	int i;
	printf ("\nNo key generated\n");
	if (*str != '\0')
		printf("Usage:%s: %s\n",prog, str);
	printf("Usage:%s -{DHR} <size> [-F] -{zhu} [-ac]  [-p <no>]"
	       " [-s <no>] -n name\n", prog);
	if (flag == 0)
		exit(2);
	printf("\t-D generate DSA/DSS KEY: size must be one of following:\n");
	printf("\t\t");
	for(i = 0; dsa_sizes[i] > 0; i++)
		printf(" %d,", dsa_sizes[i]);
	printf("\n");
	printf("\t-H generate HMAC-MD5 KEY: size in the range [1..512]:\n");
	printf("\t-R generate RSA KEY: size in the range [512..4096]\n");
	printf("\t-F RSA KEYS only: use large exponent\n");

	printf("\t-z Zone key \n");
	printf("\t-h Host/Entity key \n");
	printf("\t-u User key \n");

	printf("\t-a Key CANNOT be used for authentication\n");
	printf("\t-c Key CANNOT be used for encryption\n");

	printf("\t-p Set protocol field to <no>\n");
	printf("\t\t default: 2 (email) for Host keys, 3 (dnssec) for all others\n");
	printf("\t-s Strength value this key signs DNS records with\n");
	printf("\t\t default: 1 for Zone keys, 0 for all others\n");
	printf("\t-n name: the owner of the key\n");

	if (flag == PRINT_SUPPORTED) {
		printf("Available algorithms are:");
		if (dst_check_algorithm(NS_ALG_MD5RSA) == 1) 
			printf(" RSA");
		if (dst_check_algorithm(NS_ALG_DSS) == 1) 
			printf(" DSS");
		if (dst_check_algorithm(KEY_HMAC_MD5) == 1) 
			printf(" HMAC-MD5");
		printf("\n");
	}

	exit (-3);
}


