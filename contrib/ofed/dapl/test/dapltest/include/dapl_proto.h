/*
 * Copyright (c) 2002-2005, Network Appliance, Inc. All rights reserved.
 *
 * This Software is licensed under one of the following licenses:
 *
 * 1) under the terms of the "Common Public License 1.0" a copy of which is
 *    in the file LICENSE.txt in the root directory. The license is also
 *    available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/cpl.php.
 *
 * 2) under the terms of the "The BSD License" a copy of which is in the file
 *    LICENSE2.txt in the root directory. The license is also available from
 *    the Open Source Initiative, see
 *    http://www.opensource.org/licenses/bsd-license.php.
 *
 * 3) under the terms of the "GNU General Public License (GPL) Version 2" a 
 *    copy of which is in the file LICENSE3.txt in the root directory. The 
 *    license is also available from the Open Source Initiative, see
 *    http://www.opensource.org/licenses/gpl-license.php.
 *
 * Licensee has the right to choose one of the above licenses.
 *
 * Redistributions of source code must retain the above copyright
 * notice and one of the license notices.
 *
 * Redistributions in binary form must reproduce both the above copyright
 * notice, one of the license notices in the documentation
 * and/or other materials provided with the distribution.
 */

#ifndef __DAPL_PROTO_H__
#define __DAPL_PROTO_H__

#ifdef __KERNEL__
#include <dat2/kdat.h>
#else
#include <dat2/udat.h>

#include <ctype.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#endif

#include "dapl_mdep.h"
#include "dapl_tdep_print.h"
#include "dapl_bpool.h"
#include "dapl_client_info.h"
#include "dapl_common.h"
#include "dapl_client_info.h"
#include "dapl_execute.h"
#include "dapl_getopt.h"
#include "dapl_global.h"
#include "dapl_fft_cmd.h"
#include "dapl_fft_util.h"
#include "dapl_limit_cmd.h"
#include "dapl_memlist.h"
#include "dapl_params.h"
#include "dapl_performance_stats.h"
#include "dapl_performance_test.h"
#include "dapl_quit_cmd.h"
#include "dapl_server_info.h"
#include "dapl_tdep.h"
#include "dapl_test_data.h"
#include "dapl_transaction_cmd.h"
#include "dapl_transaction_test.h"
#include "dapl_transaction_stats.h"
#include "dapl_version.h"

#define DAT_ERROR(Type,SubType) ((DAT_RETURN)(DAT_CLASS_ERROR | Type | SubType))

/*
 * Prototypes
 */

/* dapl_bpool.c */
Bpool *         DT_BpoolAlloc (Per_Test_Data_t * pt_ptr,
			       DT_Tdep_Print_Head* phead,
			       DAT_IA_HANDLE ia_handle,
			       DAT_PZ_HANDLE pz_handle,
			       DAT_EP_HANDLE ep_handle,
			       DAT_EVD_HANDLE rmr_evd_handle,
			       DAT_COUNT seg_size,
			       DAT_COUNT num_segs,
			       DAT_COUNT alignment,
			       DAT_BOOLEAN enable_rdma_write,
			       DAT_BOOLEAN enable_rdma_read);

bool            DT_Bpool_Destroy (Per_Test_Data_t * pt_ptr,
				  DT_Tdep_Print_Head *phead,
				     Bpool * bpool_ptr);

unsigned char   *DT_Bpool_GetBuffer (Bpool * bpool_ptr, int index);
DAT_COUNT        DT_Bpool_GetBuffSize (Bpool * bpool_ptr, int index);
DAT_LMR_TRIPLET *DT_Bpool_GetIOV (Bpool * bpool_ptr, int index);
DAT_LMR_CONTEXT  DT_Bpool_GetLMR (Bpool * bpool_ptr, int index);
DAT_RMR_CONTEXT  DT_Bpool_GetRMR (Bpool * bpool_ptr, int index);

void            DT_Bpool_print (DT_Tdep_Print_Head* phead, Bpool *bpool_ptr);

/* dapl_cnxn.c */
int             get_ep_connection_state (DT_Tdep_Print_Head* phead, 
				 	 DAT_EP_HANDLE ep_handle);

/* dapl_client.c */
DAT_RETURN      DT_cs_Client (Params_t * params_ptr,
			      char *dapl_name,
			      char *server_name,
			      DAT_UINT32 total_threads);

/* dapl_client_info.c */
void            DT_Client_Info_Endian (Client_Info_t * client_info);

void            DT_Client_Info_Print (DT_Tdep_Print_Head *phead, 
				      Client_Info_t * client_info);

/* dapl_transaction_stats.c */
void            DT_init_transaction_stats (Transaction_Stats_t * transaction_stats,
					 unsigned int nums);
void            DT_transaction_stats_set_ready (DT_Tdep_Print_Head* phead, 
					Transaction_Stats_t* transaction_stats);

void            DT_transaction_stats2_set_ready (DT_Tdep_Print_Head* phead, 
					Transaction_Stats_t* transaction_stats);

bool            DT_transaction_stats_wait_for_all (DT_Tdep_Print_Head* phead,
					Transaction_Stats_t* transaction_stats);

bool            DT_transaction_stats2_wait_for_all (DT_Tdep_Print_Head* phead,
					Transaction_Stats_t* transaction_stats);

void            DT_update_transaction_stats (Transaction_Stats_t * transaction_stats,
					   unsigned int num_ops,
					   unsigned int time_ms,
					   unsigned int bytes_send,
					   unsigned int bytes_recv,
					   unsigned int bytes_rdma_read,
					   unsigned int bytes_rdma_write);

void            DT_print_transaction_stats (DT_Tdep_Print_Head* phead, 
					Transaction_Stats_t* transaction_stats,
					  unsigned int num_threads,
					  unsigned int num_EPs);

/* dapl_endian.c */
void            DT_Endian_Init (void);
DAT_UINT32      DT_Endian32 (DAT_UINT32 val);
DAT_UINT64      DT_Endian64 (DAT_UINT64 val);
DAT_UINT32      DT_EndianMemHandle (DAT_UINT32 val);
DAT_UINT64      DT_EndianMemAddress (DAT_UINT64 val);

/* dapl_getopt.c */
void            DT_mygetopt_init (mygetopt_t * opts);

int             DT_mygetopt_r (int argc,
			       char *const * argv,
			       const char *ostr,
			       mygetopt_t * opts);

/* dapl_main.c */
int             main (int argc, char *argv[]);

int             dapltest (int argc, char *argv[]);

void            Dapltest_Main_Usage (void);

/* dapl_mdep.c */
void            DT_Mdep_Init (void);
void            DT_Mdep_End (void);
bool            DT_Mdep_GetDefaultDeviceName (char *dapl_name);
void            DT_Mdep_Sleep (int msec);
void 		DT_Mdep_Schedule (void);
bool		DT_Mdep_GetCpuStat (DT_CpuStat *sys_stat);
unsigned long   DT_Mdep_GetTime (void);
double          DT_Mdep_GetCpuMhz (void);
unsigned long   DT_Mdep_GetContextSwitchNum (void);
void           *DT_Mdep_Malloc (size_t l_);
void            DT_Mdep_Free (void *a_);
bool            DT_Mdep_LockInit (DT_Mdep_LockType * lock_ptr);
void            DT_Mdep_LockDestroy (DT_Mdep_LockType * lock_ptr);
void            DT_Mdep_Lock (DT_Mdep_LockType * lock_ptr);
void            DT_Mdep_Unlock (DT_Mdep_LockType * lock_ptr);
void            DT_Mdep_Thread_Init_Attributes (Thread * thread_ptr);
void            DT_Mdep_Thread_Destroy_Attributes (Thread * thread_ptr);
bool            DT_Mdep_Thread_Start (Thread * thread_ptr);

void 			DT_Mdep_Thread_Detach (DT_Mdep_ThreadHandleType thread_id);
DT_Mdep_ThreadHandleType DT_Mdep_Thread_SELF ( void );
void 			DT_Mdep_Thread_EXIT ( void * thread_handle );
int				DT_Mdep_wait_object_init ( IN DT_WAIT_OBJECT *wait_obj);
int 			DT_Mdep_wait_object_wait (
						    IN	DT_WAIT_OBJECT *wait_obj,
						    IN  int timeout_val);
int 			DT_Mdep_wait_object_wakeup ( IN	DT_WAIT_OBJECT *wait_obj);
int 			DT_Mdep_wait_object_destroy ( IN DT_WAIT_OBJECT *wait_obj);


DT_Mdep_Thread_Start_Routine_Return_Type
		DT_Mdep_Thread_Start_Routine (void *thread_handle);

/* dapl_memlist.c */
void            DT_MemListInit (Per_Test_Data_t * pt_ptr);
void           *DT_MemListAlloc (Per_Test_Data_t * pt_ptr,
						 char *file,
						 mem_type_e t,
						 int size);
void            DT_MemListFree (Per_Test_Data_t * pt_ptr,
						void *ptr);
void            DT_PrintMemList (Per_Test_Data_t * pt_ptr);

/* dapl_netaddr.c */
bool            DT_NetAddrLookupHostAddress (DAT_IA_ADDRESS_PTR to_netaddr,
					     char *hostname);

DAT_IA_ADDRESS_PTR DT_NetAddrAlloc (Per_Test_Data_t * pt_ptr);

void            DT_NetAddrFree (Per_Test_Data_t * pt_ptr,
				DAT_IA_ADDRESS_PTR netaddr);

/* dapl_params.c */
bool            DT_Params_Parse (int argc,
				char *argv[],
				Params_t * params_ptr);

/* dapl_performance_cmd.c */
const char *    DT_PerformanceModeToString (Performance_Mode_Type mode);

bool            DT_Performance_Cmd_Init (Performance_Cmd_t * cmd);

bool            DT_Performance_Cmd_Parse (Performance_Cmd_t * cmd,
					  int my_argc,
					  char **my_argv,
					  mygetopt_t * opts);

void            DT_Performance_Cmd_Print (Performance_Cmd_t * cmd);
void            DT_Performance_Cmd_PT_Print (DT_Tdep_Print_Head* phead, 
					     Performance_Cmd_t * cmd);

void            DT_Performance_Cmd_Endian (Performance_Cmd_t * cmd);

/* dapl_performance_client.c */
DAT_RETURN      DT_Performance_Test_Client ( Params_t	*params_ptr,
					       Per_Test_Data_t * pt_ptr,
					       DAT_IA_HANDLE * ia_handle,
					       DAT_IA_ADDRESS_PTR remote);

bool            DT_Performance_Test_Client_Connect (
	DT_Tdep_Print_Head *phead,
	Performance_Test_t * test_ptr);

bool            DT_Performance_Test_Client_Exchange (
	Params_t	   *params_ptr,
	DT_Tdep_Print_Head *phead,
					Performance_Test_t *test_ptr );

/* dapl_performance_server.c */
void            DT_Performance_Test_Server (void * pt_ptr);

bool            DT_Performance_Test_Server_Connect (
	DT_Tdep_Print_Head *phead,
	Performance_Test_t * test_ptr);

bool            DT_Performance_Test_Server_Exchange (
	DT_Tdep_Print_Head *phead,
	Performance_Test_t *test_ptr);

/* dapl_performance_util.c */
bool            DT_Performance_Test_Create (Per_Test_Data_t * pt_ptr,
					   DAT_IA_HANDLE * ia_handle,
					   DAT_IA_ADDRESS_PTR remote_ia_addr,
					   DAT_BOOLEAN is_server,
					   DAT_BOOLEAN is_remote_little_endian,
					   Performance_Test_t **perf_test);

void            DT_Performance_Test_Destroy (Per_Test_Data_t	* pt_ptr,
					    Performance_Test_t *test_ptr,
					    DAT_BOOLEAN is_server);

bool            DT_performance_post_rdma_op (Performance_Ep_Context_t *ep_context,
					    DAT_EVD_HANDLE 	reqt_evd_hdl,
					    Performance_Stats_t *stats);

unsigned int   DT_performance_reap (DT_Tdep_Print_Head* phead, 
				    DAT_EVD_HANDLE evd_handle,
				    Performance_Mode_Type mode,
				    Performance_Stats_t *stats);

unsigned int   DT_performance_wait (DT_Tdep_Print_Head* phead, 
				    DAT_EVD_HANDLE evd_handle,
				    Performance_Stats_t *stats);

unsigned int   DT_performance_poll (DT_Tdep_Print_Head* phead, 
				    DAT_EVD_HANDLE evd_handle,
				    Performance_Stats_t *stats);

/* dapl_performance_stats.c */
void            DT_performance_stats_init (Performance_Stats_t * stats);

void            DT_performance_stats_record_post (Performance_Stats_t *stats,
						 unsigned long ctxt_switch_num,
						 DT_Mdep_TimeStamp ts);

void            DT_performance_stats_record_reap (Performance_Stats_t *stats,
						 unsigned long ctxt_switch_num,
						 DT_Mdep_TimeStamp ts);

void            DT_performance_stats_record_latency (Performance_Stats_t *stats,
						    DT_Mdep_TimeStamp ts);

void            DT_performance_stats_data_combine (Performance_Stats_Data_t * dest,
						  Performance_Stats_Data_t * src_a,
						  Performance_Stats_Data_t * src_b);

void            DT_performance_stats_combine (Performance_Stats_t * dest,
					     Performance_Stats_t * src_a,
					     Performance_Stats_t * src_b);

double 		DT_performance_stats_data_print (DT_Tdep_Print_Head* phead,
						Performance_Stats_Data_t* data,
						double cpu_mhz);

void            DT_performance_stats_print (Params_t * params_ptr,
					   DT_Tdep_Print_Head* phead, 
					   Performance_Stats_t * stats,
					   Performance_Cmd_t * cmd,
					   Performance_Test_t * test);


/* dapl_server.c */
void            DT_cs_Server (Params_t * params_ptr);

/* dapl_server_cmd.c */
void            DT_Server_Cmd_Init (Server_Cmd_t * Server_Cmd);

bool            DT_Server_Cmd_Parse (Server_Cmd_t * Server_Cmd,
						     int my_argc,
						     char **my_argv,
						     mygetopt_t * opts);

void            DT_Server_Cmd_Print (Server_Cmd_t * Server_Cmd);
void            DT_Server_Cmd_PT_Print (DT_Tdep_Print_Head* phead, 
					Server_Cmd_t * Server_Cmd);

void            DT_Server_Cmd_Usage (void);

/* dapl_server_info.c */
void            DT_Server_Info_Endian (Server_Info_t * server_info);

void            DT_Server_Info_Print (DT_Tdep_Print_Head* phead, 
					Server_Info_t * server_info);

/* dapl_test_data.c */
Per_Test_Data_t *DT_Alloc_Per_Test_Data (DT_Tdep_Print_Head* phead);

void 		DT_Free_Per_Test_Data (Per_Test_Data_t * pt_ptr);

/* dapl_test_util.c */
DAT_BOOLEAN     DT_query (Per_Test_Data_t *pt_ptr,
			     DAT_IA_HANDLE   ia_handle,
			     DAT_EP_HANDLE   ep_handle);

DAT_BOOLEAN     DT_post_recv_buffer (DT_Tdep_Print_Head* phead, 
    				       DAT_EP_HANDLE ep_handle,
					Bpool * bp,
					int index,
					int size);

DAT_BOOLEAN     DT_post_send_buffer (DT_Tdep_Print_Head* phead, 
    				       DAT_EP_HANDLE ep_handle,
					Bpool * bp,
					int index,
					int size);

bool            DT_conn_event_wait (DT_Tdep_Print_Head* phead,  
    				       DAT_EP_HANDLE ep_handle,
				       DAT_EVD_HANDLE evd_handle,
				       DAT_EVENT_NUMBER *event_number);

bool		DT_disco_event_wait ( DT_Tdep_Print_Head* phead, 
				    DAT_EVD_HANDLE evd_handle,
				      DAT_EP_HANDLE  *ep_handle );

bool            DT_cr_event_wait (DT_Tdep_Print_Head* phead, 
				DAT_EVD_HANDLE evd_handle,
				     DAT_CR_ARRIVAL_EVENT_DATA *cr_stat_p);

bool            DT_dto_event_reap (DT_Tdep_Print_Head* phead, 
				DAT_EVD_HANDLE evd_handle,
				      bool poll,
				      DAT_DTO_COMPLETION_EVENT_DATA *dtop);

bool            DT_dto_event_wait (DT_Tdep_Print_Head* phead, 
				DAT_EVD_HANDLE evd_handle,
				      DAT_DTO_COMPLETION_EVENT_DATA *dtop);

bool            DT_dto_event_poll (DT_Tdep_Print_Head* phead, 
				DAT_EVD_HANDLE evd_handle,
				      DAT_DTO_COMPLETION_EVENT_DATA *dtop);

bool            DT_rmr_event_wait (DT_Tdep_Print_Head* phead, 
				DAT_EVD_HANDLE evd_handle,
				DAT_RMR_BIND_COMPLETION_EVENT_DATA *rmr_ptr);

bool            DT_dto_check ( DT_Tdep_Print_Head* phead, 
				  DAT_DTO_COMPLETION_EVENT_DATA *dto_p,
				  DAT_EP_HANDLE   ep_expected,
				  DAT_COUNT       len_expected,
				  DAT_DTO_COOKIE  cookie_expected,
				  char            *message);

bool            DT_rmr_check ( DT_Tdep_Print_Head* phead, 
				  DAT_RMR_BIND_COMPLETION_EVENT_DATA*rmr_p,
				  DAT_RMR_HANDLE  rmr_expected,
				  DAT_PVOID       cookie_expected,
				  char            *message);

bool            DT_cr_check (DT_Tdep_Print_Head* phead, 
				DAT_CR_ARRIVAL_EVENT_DATA *cr_stat_p,
				DAT_PSP_HANDLE psp_handle_expected,
				DAT_CONN_QUAL  port_expected,
				DAT_CR_HANDLE *cr_handlep,
				char          *message);

/* dapl_thread.c */
void            DT_Thread_Init (Per_Test_Data_t * pt_ptr);

void            DT_Thread_End (Per_Test_Data_t * pt_ptr);

Thread         *DT_Thread_Create (Per_Test_Data_t * pt_ptr,
				  void (*fn) (void *),
				  void *param,
				  unsigned int stacksize);

void            DT_Thread_Destroy (Thread * thread_ptr,
				    Per_Test_Data_t * pt_ptr);

bool            DT_Thread_Start (Thread * thread_ptr);

/* dapl_quit_cmd.c */
void            DT_Quit_Cmd_Init (Quit_Cmd_t * cmd);

bool            DT_Quit_Cmd_Parse (Quit_Cmd_t * cmd,
				   int my_argc,
				   char **my_argv,
				   mygetopt_t * opts);

bool            DT_Quit_Cmd_Validate (Quit_Cmd_t * cmd);

void            DT_Quit_Cmd_Endian (Quit_Cmd_t * cmd,
				    bool to_wire);

void            DT_Quit_Cmd_Print (Quit_Cmd_t * cmd);
void            DT_Quit_Cmd_PT_Print (DT_Tdep_Print_Head *phead, Quit_Cmd_t * cmd);

void            DT_Quit_Cmd_Usage (void);

/* dapl_transaction_cmd.c */
void            DT_Transaction_Cmd_Init (Transaction_Cmd_t * cmd);

bool            DT_Transaction_Cmd_Parse (Transaction_Cmd_t * cmd,
					  int my_argc,
					  char **my_argv,
					  mygetopt_t * opts);

void            DT_Transaction_Cmd_Print (Transaction_Cmd_t * cmd);
void            DT_Transaction_Cmd_PT_Print (DT_Tdep_Print_Head* phead, 
					    Transaction_Cmd_t * cmd);

void            DT_Transaction_Cmd_Endian (Transaction_Cmd_t * cmd,
					   bool to_wire);
/* dapl_transaction_test.c */
DAT_RETURN      DT_Transaction_Test_Client (Per_Test_Data_t * pt_ptr,
					    DAT_IA_HANDLE ia_handle,
					    DAT_IA_ADDRESS_PTR remote);

void            DT_Transaction_Test_Server (void *params);

bool            DT_Transaction_Create_Test (Per_Test_Data_t * pt_ptr,
					    DAT_IA_HANDLE * ia_handle,
					    DAT_BOOLEAN is_server,
					    unsigned int port_num,
					    DAT_BOOLEAN remote_is_little_endian,
					    DAT_IA_ADDRESS_PTR remote_ia_addr);

void            DT_Transaction_Main (void *param);
bool            DT_Transaction_Run (DT_Tdep_Print_Head* phead, 
						 Transaction_Test_t * test_ptr);
void            DT_Transaction_Validation_Fill (DT_Tdep_Print_Head* phead, 
						 Transaction_Test_t * test_ptr,
						unsigned int iteration);
bool            DT_Transaction_Validation_Check (DT_Tdep_Print_Head* phead, 
						 Transaction_Test_t * test_ptr,
						 int iteration);
void            DT_Print_Transaction_Test (DT_Tdep_Print_Head* phead, 
					    Transaction_Test_t* test_ptr);
void            DT_Print_Transaction_Stats (DT_Tdep_Print_Head* phead, 
					    Transaction_Test_t* test_ptr);

/* dapl_transaction_util.c */
bool            DT_handle_post_recv_buf (DT_Tdep_Print_Head* phead, 
					Ep_Context_t * ep_context,
					      unsigned int num_eps,
					      int op_indx);

bool            DT_handle_send_op (DT_Tdep_Print_Head* phead, 
					Ep_Context_t * ep_context,
					unsigned int num_eps,
					int op_indx,
					bool poll);

bool            DT_handle_recv_op (DT_Tdep_Print_Head* phead, 
					Ep_Context_t * ep_context,
					unsigned int num_eps,
					int op_indx,
					bool poll,
					bool repost_recv);

bool            DT_handle_rdma_op (DT_Tdep_Print_Head* phead, 
					Ep_Context_t * ep_context,
					unsigned int num_eps,
					DT_Transfer_Type opcode,
					int op_indx,
					bool poll);

bool            DT_check_params (Per_Test_Data_t *pt_ptr,
				 char *module);

void        	DT_Test_Error (void);

/* dapl_util.c */
const char      *DT_RetToString (DAT_RETURN ret_value);

const char      *DT_TransferTypeToString (DT_Transfer_Type type);

const char      *DT_AsyncErr2Str (DAT_EVENT_NUMBER error_code);

const char      *DT_EventToSTr (DAT_EVENT_NUMBER event_code);

const char      *DT_State2Str (DAT_EP_STATE state_code);

DAT_QOS         DT_ParseQoS (char *arg);

unsigned char   *DT_AlignPtr (void * val, DAT_COUNT align);

DAT_COUNT       DT_RoundSize (DAT_COUNT val, DAT_COUNT align);

/* dapl_limit_cmd.c */
void	    DT_Limit_Cmd_Init (	Limit_Cmd_t * cmd);

bool	    DT_Limit_Cmd_Parse ( Limit_Cmd_t * cmd,
					int my_argc,
					char **my_argv,
					mygetopt_t * opts);

void	    DT_Limit_Cmd_Usage (void);

/* dapl_limit.c */
DAT_RETURN  DT_cs_Limit (Params_t *params, Limit_Cmd_t * cmd);

/* dapl_fft_cmd.c */
void	    DT_FFT_Cmd_Init ( FFT_Cmd_t * cmd);

bool	    DT_FFT_Cmd_Parse ( FFT_Cmd_t * cmd,
					int my_argc,
					char **my_argv,
					mygetopt_t * opts);

void	    DT_FFT_Cmd_Usage (void);

/* dapl_fft_test.c */
DAT_RETURN  DT_cs_FFT (Params_t *params, FFT_Cmd_t * cmd);

/* dapl_fft_hwconn.c */
void	    DT_hwconn_test (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_hwconn_case0 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_hwconn_case1 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_hwconn_case2 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_hwconn_case3 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_hwconn_case4 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_hwconn_case5 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_hwconn_case6 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_hwconn_case7 (Params_t *params_ptr, FFT_Cmd_t *cmd);

/* dapl_fft_endpoint.c */
void	    DT_endpoint_test (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_endpoint_generic (Params_t *params_ptr, FFT_Cmd_t *cmd,
				 bool destroy_pz_early);
int	    DT_endpoint_case0 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_endpoint_case1 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_endpoint_case2 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_endpoint_case3 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_endpoint_case4 (Params_t *params_ptr, FFT_Cmd_t *cmd);

/* dapl_fft_pz.c */
void	    DT_pz_test (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_pz_case0 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_pz_case1 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_pz_case2 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_pz_case3 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_pz_case4 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_pz_case5 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_pz_case6 (Params_t *params_ptr, FFT_Cmd_t *cmd);

/* dapl_fft_util.c */
void	    DT_assert_fail (DT_Tdep_Print_Head* phead, 
			    char *exp, 
			    char *file, 
			    char *baseFile, 
			    int line);
int	    DT_ia_open (DAT_NAME_PTR dev_name, DAT_IA_HANDLE *ia_handle);
int	    DT_ep_create (Params_t *params_ptr,
			  DAT_IA_HANDLE ia_handle, 
			  DAT_PZ_HANDLE pz_handle,
			  DAT_EVD_HANDLE *cr_evd,
			  DAT_EVD_HANDLE *conn_evd, 
			  DAT_EVD_HANDLE *send_evd,
			  DAT_EVD_HANDLE *recv_evd, 
			  DAT_EP_HANDLE *ep_handle);
void	    DT_fft_init_conn_struct (FFT_Connection_t *conn);
void	    DT_fft_init_client (Params_t *params_ptr,
				FFT_Cmd_t *cmd, 
				FFT_Connection_t *conn);
int	    DT_fft_destroy_conn_struct (Params_t *params_ptr, FFT_Connection_t *conn);
void	    DT_fft_init_server (Params_t *params_ptr, FFT_Cmd_t *cmd, 
				FFT_Connection_t *conn);
void	    DT_fft_listen (Params_t *params_ptr, FFT_Connection_t *conn);
int	    DT_fft_connect (Params_t *params_ptr, FFT_Connection_t *conn);

/* dapl_fft_connmgt.c */
int	    DT_connmgt_case0 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_connmgt_case1 (Params_t *params_ptr, FFT_Cmd_t *cmd);
void	    DT_connmgt_test (Params_t *params_ptr, FFT_Cmd_t *cmd);

/* dapl_fft_mem.c */
int	    DT_mem_generic (Params_t *params_ptr, FFT_Cmd_t *cmd, int flag);
int	    DT_mem_case0 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_mem_case1 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_mem_case2 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_mem_case3 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_mem_case4 (Params_t *params_ptr, FFT_Cmd_t *cmd);
void	    DT_mem_test (Params_t *params_ptr, FFT_Cmd_t *cmd);

/* dapl_fft_queryinfo.c */
int	    DT_queryinfo_basic (Params_t *params_ptr, FFT_Cmd_t *cmd,
				FFT_query_enum object_to_query,
				DAT_RETURN result_wanted);
int	    DT_queryinfo_case0 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case1 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case2 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case3 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case4 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case5 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case6 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case7 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case8 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case9 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case10 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case11 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case12 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case13 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case14 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case15 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case16 (Params_t *params_ptr, FFT_Cmd_t *cmd);
int	    DT_queryinfo_case17 (Params_t *params_ptr, FFT_Cmd_t *cmd);
void	    DT_queryinfo_test (Params_t *params_ptr, FFT_Cmd_t *cmd);

#endif  /* __DAPL_PROTO_H__ */
