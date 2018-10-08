/* Licensed to the Apache Software Foundation (ASF) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * The ASF licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "apr_redis.h"
#include "apr_poll.h"
#include "apr_version.h"
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE  512
#define LILBUFF_SIZE 64
struct apr_redis_conn_t
{
    char *buffer;
    apr_size_t blen;
    apr_pool_t *p;
    apr_pool_t *tp;
    apr_socket_t *sock;
    apr_bucket_brigade *bb;
    apr_bucket_brigade *tb;
    apr_redis_server_t *rs;
};

/* Strings for Client Commands */

#define RC_EOL "\r\n"
#define RC_EOL_LEN (sizeof(RC_EOL)-1)

#define RC_WS " "
#define RC_WS_LEN (sizeof(RC_WS)-1)

#define RC_RESP_1 "*1\r\n"
#define RC_RESP_1_LEN (sizeof(RC_RESP_1)-1)

#define RC_RESP_2 "*2\r\n"
#define RC_RESP_2_LEN (sizeof(RC_RESP_2)-1)

#define RC_RESP_3 "*3\r\n"
#define RC_RESP_3_LEN (sizeof(RC_RESP_3)-1)

#define RC_RESP_4 "*4\r\n"
#define RC_RESP_4_LEN (sizeof(RC_RESP_4)-1)

#define RC_GET "GET\r\n"
#define RC_GET_LEN (sizeof(RC_GET)-1)

#define RC_GET_SIZE "$3\r\n"
#define RC_GET_SIZE_LEN (sizeof(RC_GET_SIZE)-1)

#define RC_SET "SET\r\n"
#define RC_SET_LEN (sizeof(RC_SET)-1)

#define RC_SET_SIZE "$3\r\n"
#define RC_SET_SIZE_LEN (sizeof(RC_SET_SIZE)-1)

#define RC_SETEX "SETEX\r\n"
#define RC_SETEX_LEN (sizeof(RC_SETEX)-1)

#define RC_SETEX_SIZE "$5\r\n"
#define RC_SETEX_SIZE_LEN (sizeof(RC_SETEX_SIZE)-1)

#define RC_DEL "DEL\r\n"
#define RC_DEL_LEN (sizeof(RC_DEL)-1)

#define RC_DEL_SIZE "$3\r\n"
#define RC_DEL_SIZE_LEN (sizeof(RC_DEL_SIZE)-1)

#define RC_QUIT "QUIT\r\n"
#define RC_QUIT_LEN (sizeof(RC_QUIT)-1)

#define RC_QUIT_SIZE "$4\r\n"
#define RC_QUIT_SIZE_LEN (sizeof(RC_QUIT_SIZE)-1)

#define RC_PING "PING\r\n"
#define RC_PING_LEN (sizeof(RC_PING)-1)

#define RC_PING_SIZE "$4\r\n"
#define RC_PING_SIZE_LEN (sizeof(RC_PING_SIZE)-1)

#define RC_INFO "INFO\r\n"
#define RC_INFO_LEN (sizeof(RC_INFO)-1)

#define RC_INFO_SIZE "$4\r\n"
#define RC_INFO_SIZE_LEN (sizeof(RC_INFO_SIZE)-1)

/* Strings for Server Replies */

#define RS_STORED "+OK"
#define RS_STORED_LEN (sizeof(RS_STORED)-1)

#define RS_NOT_STORED "$-1"
#define RS_NOT_STORED_LEN (sizeof(RS_NOT_STORED)-1)

#define RS_DELETED ":1"
#define RS_DELETED_LEN (sizeof(RS_DELETED)-1)

#define RS_NOT_FOUND_GET "$-1"
#define RS_NOT_FOUND_GET_LEN (sizeof(RS_NOT_FOUND_GET)-1)

#define RS_NOT_FOUND_DEL ":0"
#define RS_NOT_FOUND_DEL_LEN (sizeof(RS_NOT_FOUND_DEL)-1)

#define RS_TYPE_STRING "$"
#define RS_TYPE_STRING_LEN (sizeof(RS_TYPE_STRING)-1)

#define RS_END "\r\n"
#define RS_END_LEN (sizeof(RS_END)-1)

static apr_status_t make_server_dead(apr_redis_t *rc,
                                     apr_redis_server_t *rs)
{
#if APR_HAS_THREADS
    apr_thread_mutex_lock(rs->lock);
#endif
    rs->status = APR_RC_SERVER_DEAD;
    rs->btime = apr_time_now();
#if APR_HAS_THREADS
    apr_thread_mutex_unlock(rs->lock);
#endif
    return APR_SUCCESS;
}

static apr_status_t make_server_live(apr_redis_t *rc,
                                     apr_redis_server_t *rs)
{
    rs->status = APR_RC_SERVER_LIVE;
    return APR_SUCCESS;
}

APU_DECLARE(apr_status_t) apr_redis_add_server(apr_redis_t *rc,
                                               apr_redis_server_t *rs)
{
    apr_status_t rv = APR_SUCCESS;

    if (rc->ntotal >= rc->nalloc) {
        return APR_ENOMEM;
    }
    rc->live_servers[rc->ntotal] = rs;
    rc->ntotal++;
    make_server_live(rc, rs);
    return rv;
}

APU_DECLARE(apr_redis_server_t *)
apr_redis_find_server_hash(apr_redis_t *rc, const apr_uint32_t hash)
{
    if (rc->server_func) {
        return rc->server_func(rc->server_baton, rc, hash);
    }
    else {
        return apr_redis_find_server_hash_default(NULL, rc, hash);
    }
}

APU_DECLARE(apr_redis_server_t *)
apr_redis_find_server_hash_default(void *baton, apr_redis_t *rc,
                                   const apr_uint32_t hash)
{
    apr_redis_server_t *rs = NULL;
    apr_uint32_t h = hash ? hash : 1;
    apr_uint32_t i = 0;
    apr_time_t curtime = 0;

    if (rc->ntotal == 0) {
        return NULL;
    }

    do {
        rs = rc->live_servers[h % rc->ntotal];
        if (rs->status == APR_RC_SERVER_LIVE) {
            break;
        }
        else {
            if (curtime == 0) {
                curtime = apr_time_now();
            }
#if APR_HAS_THREADS
            apr_thread_mutex_lock(rs->lock);
#endif
            /* Try the dead server, every 5 seconds */
            if (curtime - rs->btime > apr_time_from_sec(5)) {
                rs->btime = curtime;
                if (apr_redis_ping(rs) == APR_SUCCESS) {
                    make_server_live(rc, rs);
#if APR_HAS_THREADS
                    apr_thread_mutex_unlock(rs->lock);
#endif
                    break;
                }
            }
#if APR_HAS_THREADS
            apr_thread_mutex_unlock(rs->lock);
#endif
        }
        h++;
        i++;
    } while (i < rc->ntotal);

    if (i == rc->ntotal) {
        rs = NULL;
    }

    return rs;
}

APU_DECLARE(apr_redis_server_t *) apr_redis_find_server(apr_redis_t *rc,
                                                        const char *host,
                                                        apr_port_t port)
{
    int i;

    for (i = 0; i < rc->ntotal; i++) {
        if (strcmp(rc->live_servers[i]->host, host) == 0
            && rc->live_servers[i]->port == port) {

            return rc->live_servers[i];
        }
    }

    return NULL;
}

static apr_status_t rs_find_conn(apr_redis_server_t *rs,
                                 apr_redis_conn_t ** conn)
{
    apr_status_t rv;
    apr_bucket_alloc_t *balloc;
    apr_bucket *e;

#if APR_HAS_THREADS
    rv = apr_reslist_acquire(rs->conns, (void **) conn);
#else
    *conn = rs->conn;
    rv = APR_SUCCESS;
#endif

    if (rv != APR_SUCCESS) {
        return rv;
    }

    balloc = apr_bucket_alloc_create((*conn)->tp);
    (*conn)->bb = apr_brigade_create((*conn)->tp, balloc);
    (*conn)->tb = apr_brigade_create((*conn)->tp, balloc);

    e = apr_bucket_socket_create((*conn)->sock, balloc);
    APR_BRIGADE_INSERT_TAIL((*conn)->bb, e);

    return rv;
}

static apr_status_t rs_bad_conn(apr_redis_server_t *rs,
                                apr_redis_conn_t *conn)
{
#if APR_HAS_THREADS
    return apr_reslist_invalidate(rs->conns, conn);
#else
    return APR_SUCCESS;
#endif
}

static apr_status_t rs_release_conn(apr_redis_server_t *rs,
                                    apr_redis_conn_t *conn)
{
    apr_pool_clear(conn->tp);
#if APR_HAS_THREADS
    return apr_reslist_release(rs->conns, conn);
#else
    return APR_SUCCESS;
#endif
}

APU_DECLARE(apr_status_t) apr_redis_enable_server(apr_redis_t *rc,
                                                  apr_redis_server_t *rs)
{
    apr_status_t rv = APR_SUCCESS;

    if (rs->status == APR_RC_SERVER_LIVE) {
        return rv;
    }
    rv = make_server_live(rc, rs);
    return rv;
}

APU_DECLARE(apr_status_t) apr_redis_disable_server(apr_redis_t *rc,
                                                   apr_redis_server_t *rs)
{
    return make_server_dead(rc, rs);
}

static apr_status_t conn_connect(apr_redis_conn_t *conn)
{
    apr_status_t rv = APR_SUCCESS;
    apr_sockaddr_t *sa;
#if APR_HAVE_SOCKADDR_UN
    apr_int32_t family = conn->rs->host[0] != '/' ? APR_INET : APR_UNIX;
#else
    apr_int32_t family = APR_INET;
#endif

    rv = apr_sockaddr_info_get(&sa, conn->rs->host, family, conn->rs->port, 0,
                               conn->p);
    if (rv != APR_SUCCESS) {
        return rv;
    }

    rv = apr_socket_timeout_set(conn->sock, 1 * APR_USEC_PER_SEC);
    if (rv != APR_SUCCESS) {
        return rv;
    }

    rv = apr_socket_connect(conn->sock, sa);
    if (rv != APR_SUCCESS) {
        return rv;
    }

    rv = apr_socket_timeout_set(conn->sock,
                                conn->rs->rwto * APR_USEC_PER_SEC);
    if (rv != APR_SUCCESS) {
        return rv;
    }

    return rv;
}

static apr_status_t
rc_conn_construct(void **conn_, void *params, apr_pool_t *pool)
{
    apr_status_t rv = APR_SUCCESS;
    apr_redis_conn_t *conn;
    apr_pool_t *np;
    apr_pool_t *tp;
    apr_redis_server_t *rs = params;
#if APR_HAVE_SOCKADDR_UN
    apr_int32_t family = rs->host[0] != '/' ? APR_INET : APR_UNIX;
#else
    apr_int32_t family = APR_INET;
#endif

    rv = apr_pool_create(&np, pool);
    if (rv != APR_SUCCESS) {
        return rv;
    }

    rv = apr_pool_create(&tp, np);
    if (rv != APR_SUCCESS) {
        apr_pool_destroy(np);
        return rv;
    }

    conn = apr_palloc(np, sizeof(apr_redis_conn_t));

    conn->p = np;
    conn->tp = tp;

    rv = apr_socket_create(&conn->sock, family, SOCK_STREAM, 0, np);

    if (rv != APR_SUCCESS) {
        apr_pool_destroy(np);
        return rv;
    }

    conn->buffer = apr_palloc(conn->p, BUFFER_SIZE + 1);
    conn->blen = 0;
    conn->rs = rs;

    rv = conn_connect(conn);
    if (rv != APR_SUCCESS) {
        apr_pool_destroy(np);
    }
    else {
        *conn_ = conn;
    }

    return rv;
}

#if APR_HAS_THREADS
static apr_status_t
rc_conn_destruct(void *conn_, void *params, apr_pool_t *pool)
{
    apr_redis_conn_t *conn = (apr_redis_conn_t *) conn_;
    struct iovec vec[3];
    apr_size_t written;

    /* send a quit message to the Redis server to be nice about it. */

    /*
     * RESP Command:
     *   *1
     *   $4
     *   QUIT
     */
    vec[0].iov_base = RC_RESP_1;
    vec[0].iov_len = RC_RESP_1_LEN;

    vec[1].iov_base = RC_QUIT_SIZE;
    vec[1].iov_len = RC_QUIT_SIZE_LEN;

    vec[2].iov_base = RC_QUIT;
    vec[2].iov_len = RC_QUIT_LEN;

    /* Return values not checked, since we just want to make it go away. */
    apr_socket_sendv(conn->sock, vec, 3, &written);
    apr_socket_close(conn->sock);

    apr_pool_destroy(conn->p);

    return APR_SUCCESS;
}
#endif

APU_DECLARE(apr_status_t) apr_redis_server_create(apr_pool_t *p,
                                                  const char *host,
                                                  apr_port_t port,
                                                  apr_uint32_t min,
                                                  apr_uint32_t smax,
                                                  apr_uint32_t max,
                                                  apr_uint32_t ttl,
                                                  apr_uint32_t rwto,
                                                  apr_redis_server_t **rs)
{
    apr_status_t rv = APR_SUCCESS;
    apr_redis_server_t *server;
    apr_pool_t *np;

    rv = apr_pool_create(&np, p);

    server = apr_palloc(np, sizeof(apr_redis_server_t));

    server->p = np;
    server->host = apr_pstrdup(np, host);
    server->port = port;
    server->status = APR_RC_SERVER_DEAD;
    server->rwto = rwto;
    server->version.major = 0;
    server->version.minor = 0;
    server->version.patch = 0;

#if APR_HAS_THREADS
    rv = apr_thread_mutex_create(&server->lock, APR_THREAD_MUTEX_DEFAULT, np);
    if (rv != APR_SUCCESS) {
        return rv;
    }

    rv = apr_reslist_create(&server->conns,
                            min,        /* hard minimum */
                            smax,       /* soft maximum */
                            max,        /* hard maximum */
                            ttl,        /* Time to live */
                            rc_conn_construct,  /* Make a New Connection */
                            rc_conn_destruct,   /* Kill Old Connection */
                            server, np);
    if (rv != APR_SUCCESS) {
        return rv;
    }

    apr_reslist_cleanup_order_set(server->conns, APR_RESLIST_CLEANUP_FIRST);
#else
    rv = rc_conn_construct((void **) &(server->conn), server, np);
    if (rv != APR_SUCCESS) {
        return rv;
    }
#endif

    *rs = server;

    return rv;
}

APU_DECLARE(apr_status_t) apr_redis_create(apr_pool_t *p,
                                           apr_uint16_t max_servers,
                                           apr_uint32_t flags,
                                           apr_redis_t **redis)
{
    apr_status_t rv = APR_SUCCESS;
    apr_redis_t *rc;

    rc = apr_palloc(p, sizeof(apr_redis_t));
    rc->p = p;
    rc->nalloc = max_servers;
    rc->ntotal = 0;
    rc->live_servers =
        apr_palloc(p, rc->nalloc * sizeof(struct apr_redis_server_t *));
    rc->hash_func = NULL;
    rc->hash_baton = NULL;
    rc->server_func = NULL;
    rc->server_baton = NULL;
    *redis = rc;
    return rv;
}


/* The crc32 functions and data was originally written by Spencer
 * Garrett <srg@quick.com> and was gleaned from the PostgreSQL source
 * tree via the files contrib/ltree/crc32.[ch] and from FreeBSD at
 * src/usr.bin/cksum/crc32.c.
 */

static const apr_uint32_t crc32tab[256] = {
    0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
    0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
    0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
    0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
    0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de,
    0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
    0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec,
    0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
    0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
    0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
    0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940,
    0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
    0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116,
    0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
    0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
    0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
    0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a,
    0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
    0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818,
    0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
    0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
    0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
    0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c,
    0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
    0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2,
    0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
    0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
    0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
    0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086,
    0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
    0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4,
    0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
    0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
    0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
    0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8,
    0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
    0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe,
    0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
    0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
    0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
    0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252,
    0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
    0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60,
    0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
    0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
    0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
    0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04,
    0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
    0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a,
    0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
    0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
    0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
    0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e,
    0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
    0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c,
    0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
    0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
    0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
    0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0,
    0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
    0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6,
    0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
    0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
    0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d,
};

APU_DECLARE(apr_uint32_t) apr_redis_hash_crc32(void *baton,
                                               const char *data,
                                               const apr_size_t data_len)
{
    apr_uint32_t i;
    apr_uint32_t crc;
    crc = ~0;

    for (i = 0; i < data_len; i++)
        crc = (crc >> 8) ^ crc32tab[(crc ^ (data[i])) & 0xff];

    return ~crc;
}

APU_DECLARE(apr_uint32_t) apr_redis_hash_default(void *baton,
                                                 const char *data,
                                                 const apr_size_t data_len)
{
    /* The default Perl Client doesn't actually use just crc32 -- it shifts it again
     * like this....
     */
    return ((apr_redis_hash_crc32(baton, data, data_len) >> 16) & 0x7fff);
}

APU_DECLARE(apr_uint32_t) apr_redis_hash(apr_redis_t *rc,
                                         const char *data,
                                         const apr_size_t data_len)
{
    if (rc->hash_func) {
        return rc->hash_func(rc->hash_baton, data, data_len);
    }
    else {
        return apr_redis_hash_default(NULL, data, data_len);
    }
}

static apr_status_t get_server_line(apr_redis_conn_t *conn)
{
    apr_size_t bsize = BUFFER_SIZE;
    apr_status_t rv = APR_SUCCESS;

    rv = apr_brigade_split_line(conn->tb, conn->bb, APR_BLOCK_READ,
            BUFFER_SIZE);

    if (rv != APR_SUCCESS) {
        return rv;
    }

    rv = apr_brigade_flatten(conn->tb, conn->buffer, &bsize);

    if (rv != APR_SUCCESS) {
        return rv;
    }

    conn->blen = bsize;
    conn->buffer[bsize] = '\0';

    return apr_brigade_cleanup(conn->tb);
}

APU_DECLARE(apr_status_t) apr_redis_set(apr_redis_t *rc,
                                        const char *key,
                                        char *data,
                                        const apr_size_t data_size,
                                        apr_uint16_t flags)
{
    apr_uint32_t hash;
    apr_redis_server_t *rs;
    apr_redis_conn_t *conn;
    apr_status_t rv;
    apr_size_t written;
    struct iovec vec[9];
    char keysize_str[LILBUFF_SIZE];
    char datasize_str[LILBUFF_SIZE];
    apr_size_t len, klen;

    klen = strlen(key);
    hash = apr_redis_hash(rc, key, klen);

    rs = apr_redis_find_server_hash(rc, hash);

    if (rs == NULL)
        return APR_NOTFOUND;

    rv = rs_find_conn(rs, &conn);

    if (rv != APR_SUCCESS) {
        apr_redis_disable_server(rc, rs);
        return rv;
    }

    /*
     * RESP Command:
     *   *3
     *   $3
     *   SET
     *   $<keylen>
     *   key
     *   $<datalen>
     *   data
     */

    vec[0].iov_base = RC_RESP_3;
    vec[0].iov_len = RC_RESP_3_LEN;

    vec[1].iov_base = RC_SET_SIZE;
    vec[1].iov_len = RC_SET_SIZE_LEN;

    vec[2].iov_base = RC_SET;
    vec[2].iov_len = RC_SET_LEN;

    len = apr_snprintf(keysize_str, LILBUFF_SIZE, "$%" APR_SIZE_T_FMT "\r\n", klen);
    vec[3].iov_base = keysize_str;
    vec[3].iov_len = len;

    vec[4].iov_base = (void *) key;
    vec[4].iov_len = klen;

    vec[5].iov_base = RC_EOL;
    vec[5].iov_len = RC_EOL_LEN;

    len = apr_snprintf(datasize_str, LILBUFF_SIZE, "$%" APR_SIZE_T_FMT "\r\n",
                     data_size);
    vec[6].iov_base = datasize_str;
    vec[6].iov_len = len;

    vec[7].iov_base = data;
    vec[7].iov_len = data_size;

    vec[8].iov_base = RC_EOL;
    vec[8].iov_len = RC_EOL_LEN;

    rv = apr_socket_sendv(conn->sock, vec, 9, &written);

    if (rv != APR_SUCCESS) {
        rs_bad_conn(rs, conn);
        apr_redis_disable_server(rc, rs);
        return rv;
    }

    rv = get_server_line(conn);
    if (rv != APR_SUCCESS) {
        rs_bad_conn(rs, conn);
        apr_redis_disable_server(rc, rs);
        return rv;
    }

    if (strcmp(conn->buffer, RS_STORED RC_EOL) == 0) {
        rv = APR_SUCCESS;
    }
    else if (strcmp(conn->buffer, RS_NOT_STORED RC_EOL) == 0) {
        rv = APR_EEXIST;
    }
    else {
        rv = APR_EGENERAL;
    }

    rs_release_conn(rs, conn);
    return rv;
}

APU_DECLARE(apr_status_t) apr_redis_setex(apr_redis_t *rc,
                                          const char *key,
                                          char *data,
                                          const apr_size_t data_size,
                                          apr_uint32_t timeout,
                                          apr_uint16_t flags)
{
    apr_uint32_t hash;
    apr_redis_server_t *rs;
    apr_redis_conn_t *conn;
    apr_status_t rv;
    apr_size_t written;
    struct iovec vec[11];
    char keysize_str[LILBUFF_SIZE];
    char expire_str[LILBUFF_SIZE];
    char expiresize_str[LILBUFF_SIZE];
    char datasize_str[LILBUFF_SIZE];
    apr_size_t len, klen, expire_len;


    klen = strlen(key);
    hash = apr_redis_hash(rc, key, klen);

    rs = apr_redis_find_server_hash(rc, hash);

    if (rs == NULL)
        return APR_NOTFOUND;

    rv = rs_find_conn(rs, &conn);

    if (rv != APR_SUCCESS) {
        apr_redis_disable_server(rc, rs);
        return rv;
    }

    /*
     * RESP Command:
     *   *4
     *   $5
     *   SETEX
     *   $<keylen>
     *   key
     *   $<expirelen>
     *   expirey
     *   $<datalen>
     *   data
     */

    vec[0].iov_base = RC_RESP_4;
    vec[0].iov_len = RC_RESP_4_LEN;

    vec[1].iov_base = RC_SETEX_SIZE;
    vec[1].iov_len = RC_SETEX_SIZE_LEN;

    vec[2].iov_base = RC_SETEX;
    vec[2].iov_len = RC_SETEX_LEN;

    len = apr_snprintf(keysize_str, LILBUFF_SIZE, "$%" APR_SIZE_T_FMT "\r\n", klen);
    vec[3].iov_base = keysize_str;
    vec[3].iov_len = len;

    vec[4].iov_base = (void *) key;
    vec[4].iov_len = klen;

    vec[5].iov_base = RC_EOL;
    vec[5].iov_len = RC_EOL_LEN;

    expire_len = apr_snprintf(expire_str, LILBUFF_SIZE, "%u\r\n", timeout);
    len = apr_snprintf(expiresize_str, LILBUFF_SIZE, "$%" APR_SIZE_T_FMT "\r\n",
                     expire_len - 2);
    vec[6].iov_base = (void *) expiresize_str;
    vec[6].iov_len = len;

    vec[7].iov_base = (void *) expire_str;
    vec[7].iov_len = expire_len;

    len = apr_snprintf(datasize_str, LILBUFF_SIZE, "$%" APR_SIZE_T_FMT "\r\n",
                     data_size);
    vec[8].iov_base = datasize_str;
    vec[8].iov_len = len;

    vec[9].iov_base = data;
    vec[9].iov_len = data_size;

    vec[10].iov_base = RC_EOL;
    vec[10].iov_len = RC_EOL_LEN;

    rv = apr_socket_sendv(conn->sock, vec, 11, &written);

    if (rv != APR_SUCCESS) {
        rs_bad_conn(rs, conn);
        apr_redis_disable_server(rc, rs);
        return rv;
    }

    rv = get_server_line(conn);
    if (rv != APR_SUCCESS) {
        rs_bad_conn(rs, conn);
        apr_redis_disable_server(rc, rs);
        return rv;
    }

    if (strcmp(conn->buffer, RS_STORED RC_EOL) == 0) {
        rv = APR_SUCCESS;
    }
    else if (strcmp(conn->buffer, RS_NOT_STORED RC_EOL) == 0) {
        rv = APR_EEXIST;
    }
    else {
        rv = APR_EGENERAL;
    }

    rs_release_conn(rs, conn);
    return rv;
}

static apr_status_t grab_bulk_resp(apr_redis_server_t *rs, apr_redis_t *rc,
                                   apr_redis_conn_t *conn, apr_pool_t *p,
                                   char **baton, apr_size_t *new_length)
{
    char *length;
    char *last;
    apr_status_t rv;
    apr_size_t len = 0;
    *new_length = 0;

    length = apr_strtok(conn->buffer + 1, " ", &last);
    if (length) {
        len = strtol(length, (char **) NULL, 10);
    }

    if (len == 0) {
        *new_length = 0;
        *baton = NULL;
    }
    else {
        apr_bucket_brigade *bbb;
        apr_bucket *e;

        /* eat the trailing \r\n */
        rv = apr_brigade_partition(conn->bb, len + 2, &e);

        if (rv != APR_SUCCESS) {
            rs_bad_conn(rs, conn);
            if (rc)
                apr_redis_disable_server(rc, rs);
            return rv;
        }

        bbb = apr_brigade_split(conn->bb, e);

        rv = apr_brigade_pflatten(conn->bb, baton, &len, p);

        if (rv != APR_SUCCESS) {
            rs_bad_conn(rs, conn);
            if (rc)
                apr_redis_disable_server(rc, rs);
            return rv;
        }

        rv = apr_brigade_destroy(conn->bb);
        if (rv != APR_SUCCESS) {
            rs_bad_conn(rs, conn);
            if (rc)
                apr_redis_disable_server(rc, rs);
            return rv;
        }

        conn->bb = bbb;

        *new_length = len - 2;
        (*baton)[*new_length] = '\0';
    }
    return APR_SUCCESS;

}

APU_DECLARE(apr_status_t) apr_redis_getp(apr_redis_t *rc,
                                         apr_pool_t *p,
                                         const char *key,
                                         char **baton,
                                         apr_size_t *new_length,
                                         apr_uint16_t *flags)
{
    apr_status_t rv;
    apr_redis_server_t *rs;
    apr_redis_conn_t *conn;
    apr_uint32_t hash;
    apr_size_t written;
    apr_size_t len, klen;
    struct iovec vec[6];
    char keysize_str[LILBUFF_SIZE];

    klen = strlen(key);
    hash = apr_redis_hash(rc, key, klen);
    rs = apr_redis_find_server_hash(rc, hash);

    if (rs == NULL)
        return APR_NOTFOUND;

    rv = rs_find_conn(rs, &conn);

    if (rv != APR_SUCCESS) {
        apr_redis_disable_server(rc, rs);
        return rv;
    }

    /*
     * RESP Command:
     *   *2
     *   $3
     *   GET
     *   $<keylen>
     *   key
     */
    vec[0].iov_base = RC_RESP_2;
    vec[0].iov_len = RC_RESP_2_LEN;

    vec[1].iov_base = RC_GET_SIZE;
    vec[1].iov_len = RC_GET_SIZE_LEN;

    vec[2].iov_base = RC_GET;
    vec[2].iov_len = RC_GET_LEN;

    len = apr_snprintf(keysize_str, LILBUFF_SIZE, "$%" APR_SIZE_T_FMT "\r\n",
                     klen);
    vec[3].iov_base = keysize_str;
    vec[3].iov_len = len;

    vec[4].iov_base = (void *) key;
    vec[4].iov_len = klen;

    vec[5].iov_base = RC_EOL;
    vec[5].iov_len = RC_EOL_LEN;

    rv = apr_socket_sendv(conn->sock, vec, 6, &written);


    if (rv != APR_SUCCESS) {
        rs_bad_conn(rs, conn);
        apr_redis_disable_server(rc, rs);
        return rv;
    }

    rv = get_server_line(conn);
    if (rv != APR_SUCCESS) {
        rs_bad_conn(rs, conn);
        apr_redis_disable_server(rc, rs);
        return rv;
    }
    if (strncmp(RS_NOT_FOUND_GET, conn->buffer, RS_NOT_FOUND_GET_LEN) == 0) {
        rv = APR_NOTFOUND;
    }
    else if (strncmp(RS_TYPE_STRING, conn->buffer, RS_TYPE_STRING_LEN) == 0) {
        rv = grab_bulk_resp(rs, rc, conn, p, baton, new_length);
    }
    else {
        rv = APR_EGENERAL;
    }

    rs_release_conn(rs, conn);
    return rv;
}

APU_DECLARE(apr_status_t)
    apr_redis_delete(apr_redis_t *rc, const char *key, apr_uint32_t timeout)
{
    apr_status_t rv;
    apr_redis_server_t *rs;
    apr_redis_conn_t *conn;
    apr_uint32_t hash;
    apr_size_t written;
    struct iovec vec[6];
    apr_size_t len, klen;
    char keysize_str[LILBUFF_SIZE];

    klen = strlen(key);
    hash = apr_redis_hash(rc, key, klen);
    rs = apr_redis_find_server_hash(rc, hash);
    if (rs == NULL)
        return APR_NOTFOUND;

    rv = rs_find_conn(rs, &conn);

    if (rv != APR_SUCCESS) {
        apr_redis_disable_server(rc, rs);
        return rv;
    }

    /*
     * RESP Command:
     *   *2
     *   $3
     *   DEL
     *   $<keylen>
     *   key
     */
    vec[0].iov_base = RC_RESP_2;
    vec[0].iov_len = RC_RESP_2_LEN;

    vec[1].iov_base = RC_DEL_SIZE;
    vec[1].iov_len = RC_DEL_SIZE_LEN;

    vec[2].iov_base = RC_DEL;
    vec[2].iov_len = RC_DEL_LEN;

    len = apr_snprintf(keysize_str, LILBUFF_SIZE, "$%" APR_SIZE_T_FMT "\r\n",
                     klen);
    vec[3].iov_base = keysize_str;
    vec[3].iov_len = len;

    vec[4].iov_base = (void *) key;
    vec[4].iov_len = klen;

    vec[5].iov_base = RC_EOL;
    vec[5].iov_len = RC_EOL_LEN;

    rv = apr_socket_sendv(conn->sock, vec, 6, &written);

    if (rv != APR_SUCCESS) {
        rs_bad_conn(rs, conn);
        apr_redis_disable_server(rc, rs);
        return rv;
    }

    rv = get_server_line(conn);
    if (rv != APR_SUCCESS) {
        rs_bad_conn(rs, conn);
        apr_redis_disable_server(rc, rs);
        return rv;
    }

    if (strncmp(RS_DELETED, conn->buffer, RS_DELETED_LEN) == 0) {
        rv = APR_SUCCESS;
    }
    else if (strncmp(RS_NOT_FOUND_DEL, conn->buffer, RS_NOT_FOUND_DEL_LEN) == 0) {
        rv = APR_NOTFOUND;
    }
    else {
        rv = APR_EGENERAL;
    }

    rs_release_conn(rs, conn);
    return rv;
}

APU_DECLARE(apr_status_t)
apr_redis_ping(apr_redis_server_t *rs)
{
    apr_status_t rv;
    apr_size_t written;
    struct iovec vec[3];
    apr_redis_conn_t *conn;

    rv = rs_find_conn(rs, &conn);

    if (rv != APR_SUCCESS) {
        return rv;
    }

    /*
     * RESP Command:
     *   *1
     *   $4
     *   PING
     */
    vec[0].iov_base = RC_RESP_1;
    vec[0].iov_len = RC_RESP_1_LEN;

    vec[1].iov_base = RC_PING_SIZE;
    vec[1].iov_len = RC_PING_SIZE_LEN;

    vec[2].iov_base = RC_PING;
    vec[2].iov_len = RC_PING_LEN;

    rv = apr_socket_sendv(conn->sock, vec, 3, &written);

    if (rv != APR_SUCCESS) {
        rs_bad_conn(rs, conn);
        return rv;
    }

    rv = get_server_line(conn);
    if (rv == APR_SUCCESS) {
        /* we got *something*. Was it Redis? */
        if (strncmp(conn->buffer, "+PONG", sizeof("+PONG")-1) != 0) {
            rv = APR_EGENERAL;
        }
    }
    rs_release_conn(rs, conn);
    return rv;
}

APU_DECLARE(apr_status_t)
apr_redis_info(apr_redis_server_t *rs, apr_pool_t *p, char **baton)
{
    apr_status_t rv;
    apr_redis_conn_t *conn;
    apr_size_t written;
    struct iovec vec[3];

    rv = rs_find_conn(rs, &conn);

    if (rv != APR_SUCCESS) {
        return rv;
    }

    /*
     * RESP Command:
     *   *1
     *   $4
     *   INFO
     */
    vec[0].iov_base = RC_RESP_1;
    vec[0].iov_len = RC_RESP_1_LEN;

    vec[1].iov_base = RC_INFO_SIZE;
    vec[1].iov_len = RC_INFO_SIZE_LEN;

    vec[2].iov_base = RC_INFO;
    vec[2].iov_len = RC_INFO_LEN;

    rv = apr_socket_sendv(conn->sock, vec, 3, &written);

    if (rv != APR_SUCCESS) {
        rs_bad_conn(rs, conn);
        return rv;
    }

    rv = get_server_line(conn);
    if (rv != APR_SUCCESS) {
        rs_bad_conn(rs, conn);
        return rv;
    }

    if (strncmp(RS_TYPE_STRING, conn->buffer, RS_TYPE_STRING_LEN) == 0) {
        apr_size_t nl;
        rv = grab_bulk_resp(rs, NULL, conn, p, baton, &nl);
    } else {
        rs_bad_conn(rs, conn);
        rv = APR_EGENERAL;
    }

    rs_release_conn(rs, conn);
    return rv;
}

#define RV_FIELD "redis_version:"
APU_DECLARE(apr_status_t)
apr_redis_version(apr_redis_server_t *rs, apr_pool_t *p, char **baton)
{
    apr_status_t rv;
    char *ptr, *eptr;
    apr_pool_t *subpool;

    /* Have we already obtained the version number? */
    if (rs->version.minor != 0) {
        if (baton)
            *baton = apr_pstrdup(p, rs->version.number);
        return APR_SUCCESS;
    }
    if (apr_pool_create(&subpool, p) != APR_SUCCESS) {
        /* well, we tried */
        subpool = p;
    }
    rv = apr_redis_info(rs, subpool, baton);

    if (rv != APR_SUCCESS) {
        if (subpool != p) {
            apr_pool_destroy(subpool);
        }
        return rv;
    }

    ptr = strstr(*baton, RV_FIELD);
    if (ptr) {
        rs->version.major = strtol(ptr + sizeof(RV_FIELD) - 1, &eptr, 10);
        ptr = eptr + 1;
        rs->version.minor = strtol(ptr, &eptr, 10);
        ptr = eptr + 1;
        rs->version.patch = strtol(ptr, &eptr, 10);
        rs->version.number = apr_psprintf(rs->p, "%d.%d.%d",
                rs->version.major, rs->version.minor,
                rs->version.patch);
    }
    if (baton)
        *baton = apr_pstrdup(p, rs->version.number);
    if (subpool != p) {
        apr_pool_destroy(subpool);
    }
    return APR_SUCCESS;
}

static apr_status_t plus_minus(apr_redis_t *rc,
                    int incr,
                    const char *key,
                    apr_int32_t inc,
                    apr_uint32_t *new_value)
{
    apr_status_t rv;
    apr_redis_server_t *rs;
    apr_redis_conn_t *conn;
    apr_uint32_t hash;
    apr_size_t written;
    apr_size_t len, klen;
    struct iovec vec[12];
    char keysize_str[LILBUFF_SIZE];
    char inc_str[LILBUFF_SIZE];
    char inc_str_len[LILBUFF_SIZE];
    int i = 0;

    klen = strlen(key);
    hash = apr_redis_hash(rc, key, klen);
    rs = apr_redis_find_server_hash(rc, hash);
    if (rs == NULL)
        return APR_NOTFOUND;

    rv = rs_find_conn(rs, &conn);

    if (rv != APR_SUCCESS) {
        apr_redis_disable_server(rc, rs);
        return rv;
    }

    /*
     * RESP Command:
     *   *2|*3
     *   $4|$6
     *   INCR/DECR|INCRBY/DECRBY
     *   $<keylen>
     *   key
     *   <:inc>
     */
    if (inc == 1) {
        vec[i].iov_base = RC_RESP_2;
        vec[i].iov_len = RC_RESP_2_LEN;
        i++;

        vec[i].iov_base = "$4\r\n";
        vec[i].iov_len = sizeof("$4\r\n")-1;
        i++;

        if (incr)
            vec[i].iov_base = "INCR\r\n";
        else
            vec[i].iov_base = "DECR\r\n";
        vec[i].iov_len = sizeof("INCR\r\n")-1;
        i++;
    }
    else {
        vec[i].iov_base = RC_RESP_3;
        vec[i].iov_len = RC_RESP_3_LEN;
        i++;

        vec[i].iov_base = "$6\r\n";
        vec[i].iov_len = sizeof("$6\r\n")-1;
        i++;

       if (incr)
            vec[i].iov_base = "INCRBY\r\n";
        else
            vec[i].iov_base = "DECRBY\r\n";
        vec[i].iov_len = sizeof("INCRBY\r\n")-1;
        i++;
    }

    len = apr_snprintf(keysize_str, LILBUFF_SIZE, "$%" APR_SIZE_T_FMT "\r\n",
                     klen);
    vec[i].iov_base = keysize_str;
    vec[i].iov_len = len;
    i++;

    vec[i].iov_base = (void *) key;
    vec[i].iov_len = klen;
    i++;

    vec[i].iov_base = RC_EOL;
    vec[i].iov_len = RC_EOL_LEN;
    i++;

    if (inc != 1) {
        len = apr_snprintf(inc_str, LILBUFF_SIZE, "%d\r\n", inc);
        klen = apr_snprintf(inc_str_len, LILBUFF_SIZE, "$%d\r\n", (int)(len-2));
        vec[i].iov_base = inc_str_len;
        vec[i].iov_len = klen;
        i++;

        vec[i].iov_base = inc_str;
        vec[i].iov_len = len;
        i++;

        vec[i].iov_base = RC_EOL;
        vec[i].iov_len = RC_EOL_LEN;
        i++;
    }

    rv = apr_socket_sendv(conn->sock, vec, i, &written);

    if (rv != APR_SUCCESS) {
        rs_bad_conn(rs, conn);
        apr_redis_disable_server(rc, rs);
        return rv;
    }

    rv = get_server_line(conn);
    if (rv != APR_SUCCESS) {
        rs_bad_conn(rs, conn);
        apr_redis_disable_server(rc, rs);
        return rv;
    }
    if (strncmp(RS_NOT_FOUND_GET, conn->buffer, RS_NOT_FOUND_GET_LEN) == 0) {
        rv = APR_NOTFOUND;
    }
    else if (*conn->buffer == ':') {
        *new_value = atoi((const char *)(conn->buffer + 1));
        rv = APR_SUCCESS;
    }
    else {
        rv = APR_EGENERAL;
    }
    rs_release_conn(rs, conn);
    return rv;
}

APU_DECLARE(apr_status_t)
apr_redis_incr(apr_redis_t *rc, const char *key, apr_int32_t inc, apr_uint32_t *new_value)
{
    return plus_minus(rc, 1, key, inc, new_value);
}

APU_DECLARE(apr_status_t)
apr_redis_decr(apr_redis_t *rc, const char *key, apr_int32_t inc, apr_uint32_t *new_value)
{
    return plus_minus(rc, 0, key, inc, new_value);
}

APU_DECLARE(apr_status_t)
apr_redis_multgetp(apr_redis_t *rc,
                   apr_pool_t *temp_pool,
                   apr_pool_t *data_pool,
                   apr_hash_t *values)
{
    return APR_ENOTIMPL;
}

/**
 * Define all of the strings for stats
 */

#define STAT_process_id "process_id:"
#define STAT_process_id_LEN (sizeof(STAT_process_id)-1)

#define STAT_uptime_in_seconds "uptime_in_seconds:"
#define STAT_uptime_in_seconds_LEN (sizeof(STAT_uptime_in_seconds)-1)

#define STAT_arch_bits "arch_bits:"
#define STAT_arch_bits_LEN (sizeof(STAT_arch_bits)-1)

#define STAT_connected_clients "connected_clients:"
#define STAT_connected_clients_LEN (sizeof(STAT_connected_clients)-1)

#define STAT_blocked_clients "blocked_clients:"
#define STAT_blocked_clients_LEN (sizeof(STAT_blocked_clients)-1)

#define STAT_maxmemory "maxmemory:"
#define STAT_maxmemory_LEN (sizeof(STAT_maxmemory)-1)

#define STAT_used_memory "used_memory:"
#define STAT_used_memory_LEN (sizeof(STAT_used_memory)-1)

#define STAT_total_system_memory "total_system_memory:"
#define STAT_total_system_memory_LEN (sizeof(STAT_total_system_memory)-1)

#define STAT_total_connections_received "total_connections_received:"
#define STAT_total_connections_received_LEN (sizeof(STAT_total_connections_received)-1)

#define STAT_total_commands_processed "total_commands_processed:"
#define STAT_total_commands_processed_LEN (sizeof(STAT_total_commands_processed)-1)

#define STAT_rejected_connections "rejected_connections:"
#define STAT_rejected_connections_LEN (sizeof(STAT_rejected_connections)-1)

#define STAT_total_net_input_bytes "total_net_input_bytes:"
#define STAT_total_net_input_bytes_LEN (sizeof(STAT_total_net_input_bytes)-1)

#define STAT_total_net_output_bytes "total_net_output_bytes:"
#define STAT_total_net_output_bytes_LEN (sizeof(STAT_total_net_output_bytes)-1)

#define STAT_keyspace_hits "keyspace_hits:"
#define STAT_keyspace_hits_LEN (sizeof(STAT_keyspace_hits)-1)

#define STAT_keyspace_misses "keyspace_misses:"
#define STAT_keyspace_misses_LEN (sizeof(STAT_keyspace_misses)-1)

#define STAT_connected_slaves "connected_slaves:"
#define STAT_connected_slaves_LEN (sizeof(STAT_connected_slaves)-1)

#define STAT_used_cpu_sys "used_cpu_sys:"
#define STAT_used_cpu_sys_LEN (sizeof(STAT_used_cpu_sys)-1)

#define STAT_used_cpu_user "used_cpu_user:"
#define STAT_used_cpu_user_LEN (sizeof(STAT_used_cpu_user)-1)

#define STAT_cluster_enabled "cluster_enabled:"
#define STAT_cluster_enabled_LEN (sizeof(STAT_cluster_enabled)-1)

static apr_uint32_t stat_read_uint32( char *buf)
{
    return atoi(buf);
}

static apr_uint64_t stat_read_uint64(char *buf)
{
    return apr_atoi64(buf);
}

#define rc_do_stat(name, type) \
    if ((ptr = strstr(info , STAT_ ## name )) != NULL ) { \
        char *str = ptr + (STAT_ ## name ## _LEN ); \
        stats-> name = stat_read_ ## type (str); \
    }

static void update_stats(char *info, apr_redis_stats_t *stats)
{
    char *ptr;

    rc_do_stat(process_id, uint32);
    rc_do_stat(uptime_in_seconds, uint32);
    rc_do_stat(arch_bits, uint32);
    rc_do_stat(connected_clients, uint32);
    rc_do_stat(blocked_clients, uint32);
    rc_do_stat(maxmemory, uint64);
    rc_do_stat(used_memory, uint64);
    rc_do_stat(total_system_memory, uint64);
    rc_do_stat(total_connections_received, uint64);
    rc_do_stat(total_commands_processed, uint64);
    rc_do_stat(rejected_connections, uint64);
    rc_do_stat(total_net_input_bytes, uint64);
    rc_do_stat(total_net_output_bytes, uint64);
    rc_do_stat(keyspace_hits, uint64);
    rc_do_stat(keyspace_misses, uint64);
    rc_do_stat(connected_slaves, uint32);
    rc_do_stat(used_cpu_sys, uint32);
    rc_do_stat(used_cpu_user, uint32);
    rc_do_stat(cluster_enabled, uint32);
}

APU_DECLARE(apr_status_t)
apr_redis_stats(apr_redis_server_t *rs,
                apr_pool_t *p,
                apr_redis_stats_t **stats)
{
    apr_status_t rv;
    char *info;
    apr_pool_t *subpool;
    apr_redis_stats_t *ret;
    char *ptr;

    if (apr_pool_create(&subpool, p) != APR_SUCCESS) {
        /* well, we tried */
        subpool = p;
    }
    rv = apr_redis_info(rs, subpool, &info);

    if (rv != APR_SUCCESS) {
        if (subpool != p) {
            apr_pool_destroy(subpool);
        }
        return rv;
    }
    ret = apr_pcalloc(p, sizeof(apr_redis_stats_t));
    /* Get the bulk of the stats */
    update_stats(info, ret);

    /* Now the version number */
    if (rs->version.major != 0) {
        ret->major = rs->version.major;
        ret->minor = rs->version.minor;
        ret->patch = rs->version.patch;
    }
    else {
        char *eptr;
        ptr = strstr(info, RV_FIELD);
        if (ptr) {
            ret->major = rs->version.major = strtol(ptr + sizeof(RV_FIELD) - 1, &eptr, 10);
            ptr = eptr + 1;
            ret->minor = rs->version.minor = strtol(ptr, &eptr, 10);
            ptr = eptr + 1;
            ret->patch = rs->version.patch = strtol(ptr, &eptr, 10);
        }
    }

    /* Finally, the role */
    ptr = strstr(info, "role:");
    if (!ptr) {
        ret->role = APR_RS_SERVER_UNKNOWN;
    }
    else if (!strncmp("master", ptr + sizeof("role:") - 1, sizeof("master")-1)) {
        ret->role = APR_RS_SERVER_MASTER;
    }
    else {
        ret->role = APR_RS_SERVER_SLAVE;
    }
    if (stats) {
        *stats = ret;
    }

    return APR_SUCCESS;
}
