/*-
 * Copyright (c) 2015 Allan Jude <allanjude@FreeBSD.org>
 * Copyright (c) 2005-2011 Pawel Jakub Dawidek <pawel@dawidek.net>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "geliboot.h"

SLIST_HEAD(geli_list, geli_entry) geli_head = SLIST_HEAD_INITIALIZER(geli_head);
struct geli_list *geli_headp;

static int
geli_same_device(struct geli_entry *ge, struct dsk *dskp)
{

	if (geli_e->dsk->drive == dskp->drive &&
	    dskp->part == 255 && geli_e->dsk->part == dskp->slice) {
		/*
		 * Sometimes slice = slice, and sometimes part = slice
		 * If the incoming struct dsk has part=255, it means look at
		 * the slice instead of the part number
		 */
		return (0);
	}

	/* Is this the same device? */
	if (geli_e->dsk->drive != dskp->drive ||
	    geli_e->dsk->slice != dskp->slice ||
	    geli_e->dsk->part != dskp->part) {
		return (1);
	}

	return (0);
}

void
geli_init(void)
{

	geli_count = 0;
	SLIST_INIT(&geli_head);
}

/*
 * Read the last sector of the drive or partition pointed to by dsk and see
 * if it is GELI encrypted
 */
int
geli_taste(int read_func(void *vdev, void *priv, off_t off, void *buf,
    size_t bytes), struct dsk *dskp, daddr_t lastsector)
{
	struct g_eli_metadata md;
	u_char buf[DEV_GELIBOOT_BSIZE];
	int error;
	off_t alignsector;

	alignsector = (lastsector * DEV_BSIZE) &
	    ~(off_t)(DEV_GELIBOOT_BSIZE - 1);
	error = read_func(NULL, dskp, alignsector, &buf, DEV_GELIBOOT_BSIZE);
	if (error != 0) {
		return (error);
	}
	/* Extract the last DEV_BSIZE bytes from the block. */
	error = eli_metadata_decode(buf + (DEV_GELIBOOT_BSIZE - DEV_BSIZE),
	    &md);
	if (error != 0) {
		return (error);
	}

	if (!(md.md_flags & G_ELI_FLAG_GELIBOOT)) {
		/* The GELIBOOT feature is not activated */
		return (1);
	}
	if ((md.md_flags & G_ELI_FLAG_ONETIME)) {
		/* Swap device, skip it. */
		return (1);
	}
	if (md.md_iterations < 0) {
		/* XXX TODO: Support loading key files. */
		/* Disk does not have a passphrase, skip it. */
		return (1);
	}
	geli_e = malloc(sizeof(struct geli_entry));
	if (geli_e == NULL)
		return (2);

	geli_e->dsk = malloc(sizeof(struct dsk));
	if (geli_e->dsk == NULL)
		return (2);
	memcpy(geli_e->dsk, dskp, sizeof(struct dsk));
	geli_e->part_end = lastsector;
	if (dskp->part == 255) {
		geli_e->dsk->part = dskp->slice;
	}

	geli_e->md = md;
	eli_metadata_softc(&geli_e->sc, &md, DEV_BSIZE,
	    (lastsector + DEV_BSIZE) * DEV_BSIZE);

	SLIST_INSERT_HEAD(&geli_head, geli_e, entries);
	geli_count++;

	return (0);
}

/*
 * Attempt to decrypt the device
 */
int
geli_attach(struct dsk *dskp, const char *passphrase)
{
	u_char key[G_ELI_USERKEYLEN], mkey[G_ELI_DATAIVKEYLEN], *mkp;
	u_int keynum;
	struct hmac_ctx ctx;
	int error;

	SLIST_FOREACH_SAFE(geli_e, &geli_head, entries, geli_e_tmp) {
		if (geli_same_device(geli_e, dskp) != 0) {
			continue;
		}

		g_eli_crypto_hmac_init(&ctx, NULL, 0);
		/*
		 * Prepare Derived-Key from the user passphrase.
		 */
		if (geli_e->md.md_iterations < 0) {
			/* XXX TODO: Support loading key files. */
			return (1);
		} else if (geli_e->md.md_iterations == 0) {
			g_eli_crypto_hmac_update(&ctx, geli_e->md.md_salt,
			    sizeof(geli_e->md.md_salt));
			g_eli_crypto_hmac_update(&ctx, passphrase,
			    strlen(passphrase));
		} else if (geli_e->md.md_iterations > 0) {
			printf("Calculating GELI Decryption Key disk%dp%d @ %lu"
			    " iterations...\n", dskp->unit,
			    (dskp->slice > 0 ? dskp->slice : dskp->part),
			    geli_e->md.md_iterations);
			u_char dkey[G_ELI_USERKEYLEN];

			pkcs5v2_genkey(dkey, sizeof(dkey), geli_e->md.md_salt,
			    sizeof(geli_e->md.md_salt), passphrase,
			    geli_e->md.md_iterations);
			g_eli_crypto_hmac_update(&ctx, dkey, sizeof(dkey));
			bzero(&dkey, sizeof(dkey));
		}

		g_eli_crypto_hmac_final(&ctx, key, 0);

		error = g_eli_mkey_decrypt(&geli_e->md, key, mkey, &keynum);
		bzero(&key, sizeof(key));
		if (error == -1) {
			bzero(&mkey, sizeof(mkey));
			printf("Bad GELI key: %d\n", error);
			return (error);
		} else if (error != 0) {
			bzero(&mkey, sizeof(mkey));
			printf("Failed to decrypt GELI master key: %d\n", error);
			return (error);
		}

		/* Store the keys */
		bcopy(mkey, geli_e->sc.sc_mkey, sizeof(geli_e->sc.sc_mkey));
		bcopy(mkey, geli_e->sc.sc_ivkey, sizeof(geli_e->sc.sc_ivkey));
		mkp = mkey + sizeof(geli_e->sc.sc_ivkey);
		if ((geli_e->sc.sc_flags & G_ELI_FLAG_AUTH) == 0) {
			bcopy(mkp, geli_e->sc.sc_ekey, G_ELI_DATAKEYLEN);
		} else {
			/*
			 * The encryption key is: ekey = HMAC_SHA512(Data-Key, 0x10)
			 */
			g_eli_crypto_hmac(mkp, G_ELI_MAXKEYLEN, "\x10", 1,
			    geli_e->sc.sc_ekey, 0);
		}
		bzero(&mkey, sizeof(mkey));

		/* Initialize the per-sector IV. */
		switch (geli_e->sc.sc_ealgo) {
		case CRYPTO_AES_XTS:
			break;
		default:
			SHA256_Init(&geli_e->sc.sc_ivctx);
			SHA256_Update(&geli_e->sc.sc_ivctx, geli_e->sc.sc_ivkey,
			    sizeof(geli_e->sc.sc_ivkey));
			break;
		}

		return (0);
	}

	/* Disk not found. */
	return (2);
}

int
is_geli(struct dsk *dskp)
{
	SLIST_FOREACH_SAFE(geli_e, &geli_head, entries, geli_e_tmp) {
		if (geli_same_device(geli_e, dskp) == 0) {
			return (0);
		}
	}
	
	return (1);
}

int
geli_read(struct dsk *dskp, off_t offset, u_char *buf, size_t bytes)
{
	u_char iv[G_ELI_IVKEYLEN];
	u_char *pbuf;
	int error;
	off_t dstoff;
	uint64_t keyno;
	size_t n, nsec, secsize;
	struct g_eli_key gkey;

	pbuf = buf;
	SLIST_FOREACH_SAFE(geli_e, &geli_head, entries, geli_e_tmp) {
		if (geli_same_device(geli_e, dskp) != 0) {
			continue;
		}

		secsize = geli_e->sc.sc_sectorsize;
		nsec = bytes / secsize;
		if (nsec == 0) {
			/*
			 * A read of less than the GELI sector size has been
			 * requested. The caller provided destination buffer may
			 * not be big enough to boost the read to a full sector,
			 * so just attempt to decrypt the truncated sector.
			 */
			secsize = bytes;
			nsec = 1;
		}

		for (n = 0, dstoff = offset; n < nsec; n++, dstoff += secsize) {

			g_eli_crypto_ivgen(&geli_e->sc, dstoff, iv,
			    G_ELI_IVKEYLEN);

			/* Get the key that corresponds to this offset. */
			keyno = (dstoff >> G_ELI_KEY_SHIFT) / secsize;
			g_eli_key_fill(&geli_e->sc, &gkey, keyno);

			error = geliboot_crypt(geli_e->sc.sc_ealgo, 0, pbuf,
			    secsize, gkey.gek_key,
			    geli_e->sc.sc_ekeylen, iv);

			if (error != 0) {
				bzero(&gkey, sizeof(gkey));
				printf("Failed to decrypt in geli_read()!");
				return (error);
			}
			pbuf += secsize;
		}
		bzero(&gkey, sizeof(gkey));
		return (0);
	}

	printf("GELI provider not found\n");
	return (1);
}

int
geli_passphrase(char *pw, int disk, int parttype, int part, struct dsk *dskp)
{
	int i;

	/* TODO: Implement GELI keyfile(s) support */
	for (i = 0; i < 3; i++) {
		/* Try cached passphrase */
		if (i == 0 && pw[0] != '\0') {
			if (geli_attach(dskp, pw) == 0) {
				return (0);
			}
		}
		printf("GELI Passphrase for disk%d%c%d: ", disk, parttype, part);
		pwgets(pw, GELI_PW_MAXLEN);
		printf("\n");
		if (geli_attach(dskp, pw) == 0) {
			return (0);
		}
	}

	return (1);
}
