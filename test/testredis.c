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

#include "testutil.h"
#include "apr.h"
#include "apu.h"
#include "apr_general.h"
#include "apr_strings.h"
#include "apr_hash.h"
#include "apr_redis.h"
#include "apr_network_io.h"

#include <stdio.h>
#if APR_HAVE_STDLIB_H
#include <stdlib.h>		/* for exit() */
#endif

#define HOST "localhost"
#define PORT 6379

/* the total number of items to use for set/get testing */
#define TDATA_SIZE 3000

/* some smaller subset of TDATA_SIZE used for multiget testing */
#define TDATA_SET 100

/* our custom hash function just returns this all the time */
#define HASH_FUNC_RESULT 510

/* all keys will be prefixed with this */
static const char prefix[] = "testredis";

/* text for values we store */
static const char txt[] =
"Lorem ipsum dolor sit amet, consectetuer adipiscing elit. Duis at"
"lacus in ligula hendrerit consectetuer. Vestibulum tristique odio"
"iaculis leo. In massa arcu, ultricies a, laoreet nec, hendrerit non,"
"neque. Nulla sagittis sapien ac risus. Morbi ligula dolor, vestibulum"
"nec, viverra id, placerat dapibus, arcu. Curabitur egestas feugiat"
"tellus. Donec dignissim. Nunc ante. Curabitur id lorem. In mollis"
"tortor sit amet eros auctor dapibus. Proin nulla sem, tristique in,"
"convallis id, iaculis feugiat cras amet.";

/*
 * this datatype is for our custom server determination function. this might
 * be useful if you don't want to rely on simply hashing keys to determine
 * where a key belongs, but instead want to write something fancy, or use some
 * other kind of configuration data, i.e. a hash plus some data about a 
 * namespace, or whatever. see my_server_func, and test_redis_user_funcs
 * for the examples.
 */
typedef struct {
  const char *someval;
  apr_uint32_t which_server;
} my_hash_server_baton;


/* this could do something fancy and return some hash result. 
 * for simplicity, just return the same value, so we can test it later on.
 * if you wanted to use some external hashing library or functions for
 * consistent hashing, for example, this would be a good place to do it.
 */
static apr_uint32_t my_hash_func(void *baton, const char *data,
                                 apr_size_t data_len)
{

  return HASH_FUNC_RESULT;
}

/*
 * a fancy function to determine which server to use given some kind of data
 * and a hash value. this example actually ignores the hash value itself
 * and pulls some number from the *baton, which is a struct that has some 
 * kind of meaningful stuff in it.
 */
static apr_redis_server_t *my_server_func(void *baton,
                                             apr_redis_t *mc,
                                             const apr_uint32_t hash)
{
  apr_redis_server_t *ms = NULL;
  my_hash_server_baton *mhsb = (my_hash_server_baton *)baton;

  if(mc->ntotal == 0) {
    return NULL;
  } 

  if(mc->ntotal < mhsb->which_server) {
    return NULL;
  }

  ms = mc->live_servers[mhsb->which_server - 1];

  return ms;
}

static apr_uint16_t firsttime = 0;
static int randval(apr_uint32_t high)
{
    apr_uint32_t i = 0;
    double d = 0;

    if (firsttime == 0) {
	srand((unsigned) (getpid()));
	firsttime = 1;
    }

    d = (double) rand() / ((double) RAND_MAX + 1);
    i = (int) (d * (high - 0 + 1));

    return i > 0 ? i : 1;
}

/*
 * general test to make sure we can create the redis struct and add
 * some servers, but not more than we tell it we can add
 */

static void test_redis_create(abts_case * tc, void *data)
{
  apr_pool_t *pool = p;
  apr_status_t rv;
  apr_redis_t *redis;
  apr_redis_server_t *server, *s;
  apr_uint32_t max_servers = 10;
  apr_uint32_t i;
  apr_uint32_t hash;

  rv = apr_redis_create(pool, max_servers, 0, &redis);
  ABTS_ASSERT(tc, "redis create failed", rv == APR_SUCCESS);
  
  for (i = 1; i <= max_servers; i++) {
    apr_port_t port;
    
    port = PORT + i;
    rv =
      apr_redis_server_create(pool, HOST, PORT + i, 0, 1, 1, 60, 60, &server);
    ABTS_ASSERT(tc, "server create failed", rv == APR_SUCCESS);
    
    rv = apr_redis_add_server(redis, server);
    ABTS_ASSERT(tc, "server add failed", rv == APR_SUCCESS);
    
    s = apr_redis_find_server(redis, HOST, port);
    ABTS_PTR_EQUAL(tc, server, s);
    
    rv = apr_redis_disable_server(redis, s);
    ABTS_ASSERT(tc, "server disable failed", rv == APR_SUCCESS);
    
    rv = apr_redis_enable_server(redis, s);
    ABTS_ASSERT(tc, "server enable failed", rv == APR_SUCCESS);
    
    hash = apr_redis_hash(redis, prefix, strlen(prefix));
    ABTS_ASSERT(tc, "hash failed", hash > 0);
    
    s = apr_redis_find_server_hash(redis, hash);
    ABTS_PTR_NOTNULL(tc, s);
  }

  rv = apr_redis_server_create(pool, HOST, PORT, 0, 1, 1, 60, 60, &server);
  ABTS_ASSERT(tc, "server create failed", rv == APR_SUCCESS);
  
  rv = apr_redis_add_server(redis, server);
  ABTS_ASSERT(tc, "server add should have failed", rv != APR_SUCCESS);
  
}

/* install our own custom hashing and server selection routines. */

static int create_test_hash(apr_pool_t *p, apr_hash_t *h)
{
  int i;
  
  for (i = 0; i < TDATA_SIZE; i++) {
    char *k, *v;
    
    k = apr_pstrcat(p, prefix, apr_itoa(p, i), NULL);
    v = apr_pstrndup(p, txt, randval((apr_uint32_t)strlen(txt)));
    
    apr_hash_set(h, k, APR_HASH_KEY_STRING, v);
  }

  return i;
}

static void test_redis_user_funcs(abts_case * tc, void *data)
{
  apr_pool_t *pool = p;
  apr_status_t rv;
  apr_redis_t *redis;
  apr_redis_server_t *found;
  apr_uint32_t max_servers = 10;
  apr_uint32_t hres;
  apr_uint32_t i;
  my_hash_server_baton *baton = 
    apr_pcalloc(pool, sizeof(my_hash_server_baton));

  rv = apr_redis_create(pool, max_servers, 0, &redis);
  ABTS_ASSERT(tc, "redis create failed", rv == APR_SUCCESS);

  /* as noted above, install our custom hash function, and call 
   * apr_redis_hash. the return value should be our predefined number,
   * and our function just ignores the other args, for simplicity.
   */
  redis->hash_func = my_hash_func;

  hres = apr_redis_hash(redis, "whatever", sizeof("whatever") - 1);
  ABTS_INT_EQUAL(tc, HASH_FUNC_RESULT, hres);
  
  /* add some servers */
  for(i = 1; i <= 10; i++) {
    apr_redis_server_t *ms;

    rv = apr_redis_server_create(pool, HOST, i, 0, 1, 1, 60, 60, &ms);
    ABTS_ASSERT(tc, "server create failed", rv == APR_SUCCESS);
    
    rv = apr_redis_add_server(redis, ms);
    ABTS_ASSERT(tc, "server add failed", rv == APR_SUCCESS);
  }

  /* 
   * set 'which_server' in our server_baton to find the third server 
   * which should have the same port.
   */
  baton->which_server = 3;
  redis->server_func = my_server_func;
  redis->server_baton = baton;
  found = apr_redis_find_server_hash(redis, 0);
  ABTS_ASSERT(tc, "wrong server found", found->port == baton->which_server);
}

/* test non data related commands like stats and version */
static void test_redis_meta(abts_case * tc, void *data)
{
    apr_pool_t *pool = p;
    apr_redis_t *redis;
    apr_redis_server_t *server;
    apr_redis_stats_t *stats;
    char *result;
    apr_status_t rv;

    rv = apr_redis_create(pool, 1, 0, &redis);
    ABTS_ASSERT(tc, "redis create failed", rv == APR_SUCCESS);

    rv = apr_redis_server_create(pool, HOST, PORT, 0, 1, 1, 60, 60, &server);
    ABTS_ASSERT(tc, "server create failed", rv == APR_SUCCESS);

    rv = apr_redis_add_server(redis, server);
    ABTS_ASSERT(tc, "server add failed", rv == APR_SUCCESS);

    rv = apr_redis_version(server, pool, &result);
    ABTS_PTR_NOTNULL(tc, result);

    rv = apr_redis_stats(server, p, &stats);
    ABTS_PTR_NOTNULL(tc, stats);

    /* 
     * no way to know exactly what will be in most of these, so
     * just make sure there is something.
     */
    ABTS_ASSERT(tc, "major", stats->major >= 1);
    ABTS_ASSERT(tc, "minor", stats->minor >= 0);
    ABTS_ASSERT(tc, "patch", stats->patch >= 0);
    ABTS_ASSERT(tc, "process_id", stats->process_id >= 0);
    ABTS_ASSERT(tc, "uptime_in_seconds", stats->uptime_in_seconds >= 0);
    ABTS_ASSERT(tc, "arch_bits", stats->arch_bits >= 0);
    ABTS_ASSERT(tc, "connected_clients", stats->connected_clients >= 0);
    ABTS_ASSERT(tc, "blocked_clients", stats->blocked_clients >= 0);
    ABTS_ASSERT(tc, "maxmemory", stats->maxmemory >= 0);
    ABTS_ASSERT(tc, "used_memory", stats->used_memory >= 0);
    ABTS_ASSERT(tc, "total_system_memory", stats->total_system_memory >= 0);
    ABTS_ASSERT(tc, "total_connections_received", stats->total_connections_received >= 0);
    ABTS_ASSERT(tc, "total_commands_processed", stats->total_commands_processed >= 0);
    ABTS_ASSERT(tc, "total_net_input_bytes", stats->total_net_input_bytes >= 0);
    ABTS_ASSERT(tc, "total_net_output_bytes", stats->total_net_output_bytes >= 0);
    ABTS_ASSERT(tc, "keyspace_hits", stats->keyspace_hits >= 0);
    ABTS_ASSERT(tc, "keyspace_misses", stats->keyspace_misses >= 0);
    ABTS_ASSERT(tc, "role", stats->role >= 0);
    ABTS_ASSERT(tc, "connected_slaves", stats->connected_slaves >= 0);
    ABTS_ASSERT(tc, "used_cpu_sys", stats->used_cpu_sys >= 0);
    ABTS_ASSERT(tc, "used_cpu_user", stats->used_cpu_user >= 0);
    ABTS_ASSERT(tc, "cluster_enabled", stats->cluster_enabled >= 0);
}


/* basic tests of the increment and decrement commands */
static void test_redis_incrdecr(abts_case * tc, void *data)
{
 apr_pool_t *pool = p;
 apr_status_t rv;
 apr_redis_t *redis;
 apr_redis_server_t *server;
 apr_uint32_t new;
 char *result;
 apr_size_t len;
 apr_uint32_t i;

  rv = apr_redis_create(pool, 1, 0, &redis);
  ABTS_ASSERT(tc, "redis create failed", rv == APR_SUCCESS);
  
  rv = apr_redis_server_create(pool, HOST, PORT, 0, 1, 1, 60, 60, &server);
  ABTS_ASSERT(tc, "server create failed", rv == APR_SUCCESS);
  
  rv = apr_redis_add_server(redis, server);
  ABTS_ASSERT(tc, "server add failed", rv == APR_SUCCESS);

  rv = apr_redis_set(redis, prefix, "271", sizeof("271") - 1, 27);
  ABTS_ASSERT(tc, "set failed", rv == APR_SUCCESS);
  
  for( i = 1; i <= TDATA_SIZE; i++) {
    apr_uint32_t expect;

    rv = apr_redis_getp(redis, pool, prefix, &result, &len, NULL);
    ABTS_ASSERT(tc, "get failed", rv == APR_SUCCESS);

    expect = i + atoi(result);

    rv = apr_redis_incr(redis, prefix, i, &new);
    ABTS_ASSERT(tc, "incr failed", rv == APR_SUCCESS);

    ABTS_INT_EQUAL(tc, expect, new);

    rv = apr_redis_decr(redis, prefix, i, &new);
    ABTS_ASSERT(tc, "decr failed", rv == APR_SUCCESS);

    ABTS_INT_EQUAL(tc, atoi(result), new);

  }

  rv = apr_redis_getp(redis, pool, prefix, &result, &len, NULL);
  ABTS_ASSERT(tc, "get failed", rv == APR_SUCCESS);

  ABTS_INT_EQUAL(tc, 271, atoi(result));

  rv = apr_redis_delete(redis, prefix, 0);
  ABTS_ASSERT(tc, "delete failed", rv == APR_SUCCESS);
}


/* test setting and getting */

static void test_redis_setget(abts_case * tc, void *data)
{
    apr_pool_t *pool = p;
    apr_status_t rv;
    apr_redis_t *redis;
    apr_redis_server_t *server;
    apr_hash_t *tdata;
    apr_hash_index_t *hi;
    char *result;
    apr_size_t len;

    rv = apr_redis_create(pool, 1, 0, &redis);
    ABTS_ASSERT(tc, "redis create failed", rv == APR_SUCCESS);

    rv = apr_redis_server_create(pool, HOST, PORT, 0, 1, 1, 60, 60, &server);
    ABTS_ASSERT(tc, "server create failed", rv == APR_SUCCESS);

    rv = apr_redis_add_server(redis, server);
    ABTS_ASSERT(tc, "server add failed", rv == APR_SUCCESS);

    tdata = apr_hash_make(pool);

    create_test_hash(pool, tdata);

    for (hi = apr_hash_first(p, tdata); hi; hi = apr_hash_next(hi)) {
	const void *k;
	void *v;
        const char *key;

	apr_hash_this(hi, &k, NULL, &v);
        key = k;

	rv = apr_redis_set(redis, key, v, strlen(v), 27);
	ABTS_ASSERT(tc, "set failed", rv == APR_SUCCESS);
	rv = apr_redis_getp(redis, pool, key, &result, &len, NULL);
	ABTS_ASSERT(tc, "get failed", rv == APR_SUCCESS);
    }

    rv = apr_redis_getp(redis, pool, "nothere3423", &result, &len, NULL);

    ABTS_ASSERT(tc, "get should have failed", rv != APR_SUCCESS);

    for (hi = apr_hash_first(p, tdata); hi; hi = apr_hash_next(hi)) {
	const void *k;
	const char *key;

	apr_hash_this(hi, &k, NULL, NULL);
	key = k;

	rv = apr_redis_delete(redis, key, 0);
	ABTS_ASSERT(tc, "delete failed", rv == APR_SUCCESS);
    }
}

/* test setting and getting */

static void test_redis_setexget(abts_case * tc, void *data)
{
    apr_pool_t *pool = p;
    apr_status_t rv;
    apr_redis_t *redis;
    apr_redis_server_t *server;
    apr_hash_t *tdata;
    apr_hash_index_t *hi;
    char *result;
    apr_size_t len;

    rv = apr_redis_create(pool, 1, 0, &redis);
    ABTS_ASSERT(tc, "redis create failed", rv == APR_SUCCESS);

    rv = apr_redis_server_create(pool, HOST, PORT, 0, 1, 1, 60, 60, &server);
    ABTS_ASSERT(tc, "server create failed", rv == APR_SUCCESS);

    rv = apr_redis_add_server(redis, server);
    ABTS_ASSERT(tc, "server add failed", rv == APR_SUCCESS);

    tdata = apr_hash_make(pool);

    create_test_hash(pool, tdata);

    for (hi = apr_hash_first(p, tdata); hi; hi = apr_hash_next(hi)) {
    const void *k;
    void *v;
        const char *key;

    apr_hash_this(hi, &k, NULL, &v);
        key = k;

    rv = apr_redis_ping(server);
    ABTS_ASSERT(tc, "ping failed", rv == APR_SUCCESS);
    rv = apr_redis_setex(redis, key, v, strlen(v), 10, 27);
    ABTS_ASSERT(tc, "set failed", rv == APR_SUCCESS);
    rv = apr_redis_getp(redis, pool, key, &result, &len, NULL);
    ABTS_ASSERT(tc, "get failed", rv == APR_SUCCESS);
    }

    rv = apr_redis_getp(redis, pool, "nothere3423", &result, &len, NULL);

    ABTS_ASSERT(tc, "get should have failed", rv != APR_SUCCESS);

    for (hi = apr_hash_first(p, tdata); hi; hi = apr_hash_next(hi)) {
    const void *k;
    const char *key;

    apr_hash_this(hi, &k, NULL, NULL);
    key = k;

    rv = apr_redis_delete(redis, key, 0);
    ABTS_ASSERT(tc, "delete failed", rv == APR_SUCCESS);
    }
}

/* use apr_socket stuff to see if there is in fact a Redis server
 * running on PORT.
 */
static apr_status_t check_redis(void)
{
  apr_pool_t *pool = p;
  apr_status_t rv;
  apr_socket_t *sock = NULL;
  apr_sockaddr_t *sa;
  struct iovec vec[2];
  apr_size_t written;
  char buf[128];
  apr_size_t len;

  rv = apr_socket_create(&sock, APR_INET, SOCK_STREAM, 0, pool);
  if(rv != APR_SUCCESS) {
    return rv;
  }

  rv = apr_sockaddr_info_get(&sa, HOST, APR_INET, PORT, 0, pool);
  if(rv != APR_SUCCESS) {
    return rv;
  }

  rv = apr_socket_timeout_set(sock, 1 * APR_USEC_PER_SEC);
  if (rv != APR_SUCCESS) {
    return rv;
  }

  rv = apr_socket_connect(sock, sa);
  if (rv != APR_SUCCESS) {
    return rv;
  }

  rv = apr_socket_timeout_set(sock, -1);
  if (rv != APR_SUCCESS) {
    return rv;
  }

  vec[0].iov_base = "PING";
  vec[0].iov_len  = sizeof("PING") - 1;

  vec[1].iov_base = "\r\n";
  vec[1].iov_len  = sizeof("\r\n") -1;

  rv = apr_socket_sendv(sock, vec, 2, &written);
  if (rv != APR_SUCCESS) {
    return rv;
  }

  len = sizeof(buf);
  rv = apr_socket_recv(sock, buf, &len);
  if(rv != APR_SUCCESS) {
    return rv;
  }
  if(strncmp(buf, "+PONG", sizeof("+PONG")-1) != 0) {
    rv = APR_EGENERAL;
  }

  apr_socket_close(sock);
  return rv;
}

abts_suite *testredis(abts_suite * suite)
{
    apr_status_t rv;
    suite = ADD_SUITE(suite);
    /* check for a running redis on the typical port before
     * trying to run the tests. succeed if we don't find one.
     */
    rv = check_redis();
    if (rv == APR_SUCCESS) {
        abts_run_test(suite, test_redis_create, NULL);
        abts_run_test(suite, test_redis_user_funcs, NULL);
        abts_run_test(suite, test_redis_meta, NULL);
        abts_run_test(suite, test_redis_setget, NULL);
        abts_run_test(suite, test_redis_setexget, NULL);
        /* abts_run_test(suite, test_redis_multiget, NULL); */
        abts_run_test(suite, test_redis_incrdecr, NULL);
    }
    else {
        abts_log_message("Error %d occurred attempting to reach Redis "
                         "on %s:%d.  Skipping apr_redis tests...",
                         rv, HOST, PORT);
    }

    return suite;
}
