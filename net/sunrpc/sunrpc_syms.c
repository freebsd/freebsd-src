/*
 * linux/net/sunrpc/sunrpc_syms.c
 *
 * Symbols exported by the sunrpc module.
 *
 * Copyright (C) 1997 Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/config.h>
#include <linux/module.h>

#include <linux/types.h>
#include <linux/socket.h>
#include <linux/sched.h>
#include <linux/uio.h>
#include <linux/unistd.h>

#include <linux/sunrpc/sched.h>
#include <linux/sunrpc/clnt.h>
#include <linux/sunrpc/svc.h>
#include <linux/sunrpc/svcsock.h>
#include <linux/sunrpc/auth.h>

/* RPC scheduler */
EXPORT_SYMBOL(rpc_allocate);
EXPORT_SYMBOL(rpc_free);
EXPORT_SYMBOL(rpc_execute);
EXPORT_SYMBOL(rpc_init_task);
EXPORT_SYMBOL(rpc_sleep_on);
EXPORT_SYMBOL(rpc_wake_up_next);
EXPORT_SYMBOL(rpc_wake_up_task);
EXPORT_SYMBOL(rpc_new_child);
EXPORT_SYMBOL(rpc_run_child);
EXPORT_SYMBOL(rpciod_down);
EXPORT_SYMBOL(rpciod_up);
EXPORT_SYMBOL(rpc_new_task);
EXPORT_SYMBOL(rpc_wake_up_status);
EXPORT_SYMBOL(rpc_release_task);

/* RPC client functions */
EXPORT_SYMBOL(rpc_create_client);
EXPORT_SYMBOL(rpc_destroy_client);
EXPORT_SYMBOL(rpc_shutdown_client);
EXPORT_SYMBOL(rpc_killall_tasks);
EXPORT_SYMBOL(rpc_call_sync);
EXPORT_SYMBOL(rpc_call_async);
EXPORT_SYMBOL(rpc_call_setup);
EXPORT_SYMBOL(rpc_clnt_sigmask);
EXPORT_SYMBOL(rpc_clnt_sigunmask);
EXPORT_SYMBOL(rpc_delay);
EXPORT_SYMBOL(rpc_restart_call);
EXPORT_SYMBOL(rpc_setbufsize);

/* Client transport */
EXPORT_SYMBOL(xprt_create_proto);
EXPORT_SYMBOL(xprt_destroy);
EXPORT_SYMBOL(xprt_set_timeout);

/* Client credential cache */
EXPORT_SYMBOL(rpcauth_register);
EXPORT_SYMBOL(rpcauth_unregister);
EXPORT_SYMBOL(rpcauth_init_credcache);
EXPORT_SYMBOL(rpcauth_free_credcache);
EXPORT_SYMBOL(rpcauth_insert_credcache);
EXPORT_SYMBOL(rpcauth_lookupcred);
EXPORT_SYMBOL(rpcauth_bindcred);
EXPORT_SYMBOL(rpcauth_matchcred);
EXPORT_SYMBOL(put_rpccred);

/* RPC server stuff */
EXPORT_SYMBOL(svc_create);
EXPORT_SYMBOL(svc_create_thread);
EXPORT_SYMBOL(svc_exit_thread);
EXPORT_SYMBOL(svc_destroy);
EXPORT_SYMBOL(svc_drop);
EXPORT_SYMBOL(svc_process);
EXPORT_SYMBOL(svc_recv);
EXPORT_SYMBOL(svc_wake_up);
EXPORT_SYMBOL(svc_makesock);
EXPORT_SYMBOL(svc_reserve);

/* RPC statistics */
#ifdef CONFIG_PROC_FS
EXPORT_SYMBOL(rpc_proc_register);
EXPORT_SYMBOL(rpc_proc_unregister);
EXPORT_SYMBOL(rpc_proc_read);
EXPORT_SYMBOL(svc_proc_register);
EXPORT_SYMBOL(svc_proc_unregister);
EXPORT_SYMBOL(svc_proc_read);
#endif

/* Generic XDR */
EXPORT_SYMBOL(xdr_encode_array);
EXPORT_SYMBOL(xdr_encode_string);
EXPORT_SYMBOL(xdr_decode_string);
EXPORT_SYMBOL(xdr_decode_string_inplace);
EXPORT_SYMBOL(xdr_decode_netobj);
EXPORT_SYMBOL(xdr_encode_netobj);
EXPORT_SYMBOL(xdr_encode_pages);
EXPORT_SYMBOL(xdr_inline_pages);
EXPORT_SYMBOL(xdr_shift_buf);

/* Debugging symbols */
#ifdef RPC_DEBUG
EXPORT_SYMBOL(rpc_debug);
EXPORT_SYMBOL(nfs_debug);
EXPORT_SYMBOL(nfsd_debug);
EXPORT_SYMBOL(nlm_debug);
#endif
