/* @(#)whoami.x	2.2 88/08/22 4.0 RPCSRC */

const WHOAMI_NGROUPS = 16;

typedef string  name<MAXNETNAMELEN>;

struct remote_identity {
    bool authenticated;     /* TRUE if the server authenticates us */
    name remote_username;   /* login name */
    name remote_realname;   /* gcos-field name (long name) */
    int uid;
    int gid;
    int gids<WHOAMI_NGROUPS>;
};

program WHOAMI {
    version WHOAMI_V1 {
        /*
         * Report on the server's notion of the client's identity.
         * Will respond to AUTH_DES only.
         */
        remote_identity
        WHOAMI_IASK(void) = 1;
        /*
         * Return server's netname. AUTH_NONE is okay.
         * This routine allows this server to be started under any uid,
         * and the client can ask it its netname for use in authdes_create().
         */
        name
        WHOAMI_WHORU(void) = 2;

    } = 1;
} = 80955;
