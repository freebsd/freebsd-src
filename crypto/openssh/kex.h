/*
 * Copyright (c) 2000 Markus Friedl.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Markus Friedl.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef KEX_H
#define KEX_H

#define	KEX_DH1	"diffie-hellman-group1-sha1"
#define KEX_DSS	"ssh-dss"

enum kex_init_proposals {
	PROPOSAL_KEX_ALGS,
	PROPOSAL_SERVER_HOST_KEY_ALGS,
	PROPOSAL_ENC_ALGS_CTOS,
	PROPOSAL_ENC_ALGS_STOC,
	PROPOSAL_MAC_ALGS_CTOS,
	PROPOSAL_MAC_ALGS_STOC,
	PROPOSAL_COMP_ALGS_CTOS,
	PROPOSAL_COMP_ALGS_STOC,
	PROPOSAL_LANG_CTOS,
	PROPOSAL_LANG_STOC,
	PROPOSAL_MAX
};

enum kex_modes {
	MODE_IN,
	MODE_OUT,
	MODE_MAX
};

typedef struct Kex Kex;
typedef struct Mac Mac;
typedef struct Comp Comp;
typedef struct Enc Enc;

struct Enc {
	int		type;
	int		enabled;
	int		block_size;
	unsigned char	*key;
	unsigned char	*iv;
	int		key_len;
	int		iv_len;
	char		*name;
};
struct Mac {
	EVP_MD		*md;
	int		enabled;
	int		mac_len;
	unsigned char	*key;
	int		key_len;
	char		*name;
};
struct Comp {
	int		type;
	int		enabled;
	char		*name;
};
struct Kex {
	Enc		enc [MODE_MAX];
	Mac		mac [MODE_MAX];
	Comp		comp[MODE_MAX];
	int		we_need;
	int		server;
	char		*name;
	char		*hostkeyalg;
};

Buffer	*kex_init(char *myproposal[PROPOSAL_MAX]);
void
kex_exchange_kexinit(
    Buffer *my_kexinit, Buffer *peer_kexint,
    char *peer_proposal[PROPOSAL_MAX]);
Kex *
kex_choose_conf(char *cprop[PROPOSAL_MAX],
    char *sprop[PROPOSAL_MAX], int server);
int	kex_derive_keys(Kex *k, unsigned char *hash, BIGNUM *shared_secret);
void	packet_set_kex(Kex *k);
int	dh_pub_is_valid(DH *dh, BIGNUM *dh_pub);
DH	*dh_new_group1();

unsigned char *
kex_hash(
    char *client_version_string,
    char *server_version_string,
    char *ckexinit, int ckexinitlen,
    char *skexinit, int skexinitlen,
    char *serverhostkeyblob, int sbloblen,
    BIGNUM *client_dh_pub,
    BIGNUM *server_dh_pub,
    BIGNUM *shared_secret);

#endif
