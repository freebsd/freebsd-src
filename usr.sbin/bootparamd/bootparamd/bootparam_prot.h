#include <rpc/types.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <nfs/nfs.h>
#define MAX_MACHINE_NAME 255
#define MAX_PATH_LEN 1024
#define MAX_FILEID 32
#define IP_ADDR_TYPE 1

typedef char *bp_machine_name_t;
bool_t xdr_bp_machine_name_t();


typedef char *bp_path_t;
bool_t xdr_bp_path_t();


typedef char *bp_fileid_t;
bool_t xdr_bp_fileid_t();


struct ip_addr_t {
	char net;
	char host;
	char lh;
	char impno;
};
typedef struct ip_addr_t ip_addr_t;
bool_t xdr_ip_addr_t();


struct bp_address {
	int address_type;
	union {
		ip_addr_t ip_addr;
	} bp_address_u;
};
typedef struct bp_address bp_address;
bool_t xdr_bp_address();


struct bp_whoami_arg {
	bp_address client_address;
};
typedef struct bp_whoami_arg bp_whoami_arg;
bool_t xdr_bp_whoami_arg();


struct bp_whoami_res {
	bp_machine_name_t client_name;
	bp_machine_name_t domain_name;
	bp_address router_address;
};
typedef struct bp_whoami_res bp_whoami_res;
bool_t xdr_bp_whoami_res();


struct bp_getfile_arg {
	bp_machine_name_t client_name;
	bp_fileid_t file_id;
};
typedef struct bp_getfile_arg bp_getfile_arg;
bool_t xdr_bp_getfile_arg();


struct bp_getfile_res {
	bp_machine_name_t server_name;
	bp_address server_address;
	bp_path_t server_path;
};
typedef struct bp_getfile_res bp_getfile_res;
bool_t xdr_bp_getfile_res();


#define BOOTPARAMPROG ((u_long)100026)
#define BOOTPARAMVERS ((u_long)1)
#define BOOTPARAMPROC_WHOAMI ((u_long)1)
extern bp_whoami_res *bootparamproc_whoami_1();
#define BOOTPARAMPROC_GETFILE ((u_long)2)
extern bp_getfile_res *bootparamproc_getfile_1();

