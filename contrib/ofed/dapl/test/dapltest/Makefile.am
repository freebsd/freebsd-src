if EXT_TYPE_IB
XFLAGS = -DDAT_EXTENSIONS
else
XFLAGS =
endif

AM_CFLAGS = -g -Wall -D_GNU_SOURCE

dapltest_CFLAGS = $(AM_FLAGS) $(XFLAGS)

INCLUDES =  -I include \
	    -I mdep/linux \
	    -I $(srcdir)/../../dat/include
                  
bin_PROGRAMS = dapltest

dapltest_SOURCES =				\
	cmd/dapl_main.c				\
	cmd/dapl_params.c			\
	cmd/dapl_fft_cmd.c			\
	cmd/dapl_getopt.c			\
	cmd/dapl_limit_cmd.c			\
	cmd/dapl_netaddr.c			\
	cmd/dapl_performance_cmd.c		\
	cmd/dapl_qos_util.c			\
	cmd/dapl_quit_cmd.c			\
	cmd/dapl_server_cmd.c			\
	cmd/dapl_transaction_cmd.c		\
	test/dapl_bpool.c			\
	test/dapl_client.c			\
	test/dapl_client_info.c			\
	test/dapl_cnxn.c			\
	test/dapl_execute.c			\
	test/dapl_fft_connmgt.c			\
	test/dapl_fft_endpoint.c		\
	test/dapl_fft_hwconn.c			\
	test/dapl_fft_mem.c			\
	test/dapl_fft_pz.c			\
	test/dapl_fft_queryinfo.c		\
	test/dapl_fft_test.c			\
	test/dapl_fft_util.c			\
	test/dapl_limit.c			\
	test/dapl_memlist.c			\
	test/dapl_performance_client.c		\
	test/dapl_performance_server.c		\
	test/dapl_performance_stats.c		\
	test/dapl_performance_util.c		\
	test/dapl_quit_util.c			\
	test/dapl_server.c			\
	test/dapl_server_info.c			\
	test/dapl_test_data.c			\
	test/dapl_test_util.c			\
	test/dapl_thread.c			\
	test/dapl_transaction_stats.c		\
	test/dapl_transaction_test.c		\
	test/dapl_transaction_util.c		\
	test/dapl_util.c			\
	common/dapl_endian.c			\
	common/dapl_global.c			\
	common/dapl_performance_cmd_util.c	\
	common/dapl_quit_cmd_util.c		\
	common/dapl_transaction_cmd_util.c	\
	udapl/udapl_tdep.c			\
	mdep/linux/dapl_mdep_user.c
	
dapltest_LDADD = $(top_builddir)/dat/udat/libdat2.la
dapltest_LDFLAGS = -lpthread  
