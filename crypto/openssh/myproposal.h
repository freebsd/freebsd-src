#define KEX_DEFAULT_KEX		"diffie-hellman-group1-sha1"
#define	KEX_DEFAULT_PK_ALG	"ssh-dss"
#define	KEX_DEFAULT_ENCRYPT	"3des-cbc,blowfish-cbc,arcfour,cast128-cbc"
#define	KEX_DEFAULT_MAC		"hmac-sha1,hmac-md5,hmac-ripemd160@openssh.com"
#define	KEX_DEFAULT_COMP	"zlib,none"
#define	KEX_DEFAULT_LANG	""


static char *myproposal[PROPOSAL_MAX] = {
	KEX_DEFAULT_KEX,
	KEX_DEFAULT_PK_ALG,
	KEX_DEFAULT_ENCRYPT,
	KEX_DEFAULT_ENCRYPT,
	KEX_DEFAULT_MAC,
	KEX_DEFAULT_MAC,
	KEX_DEFAULT_COMP,
	KEX_DEFAULT_COMP,
	KEX_DEFAULT_LANG,
	KEX_DEFAULT_LANG
};
