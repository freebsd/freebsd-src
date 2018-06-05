#ifndef _BHYVE_MIGRATION_
#define _BHYVE_MIGRATION_

struct vmctx;

struct __attribute__((packed)) restore_state {
	int kdata_fd;
	int vmmem_fd;

	void *kdata_map;
	size_t kdata_len;

	size_t vmmem_len;

	struct ucl_parser *meta_parser;
	ucl_object_t *meta_root_obj;
};

struct checkpoint_thread_info {
	struct vmctx *ctx;
	int socket_fd;
	struct sockaddr_un *addr;
} checkpoint_info;

void destroy_restore_state(struct restore_state *rstate);

const char * lookup_vmname(struct restore_state *rstate);
int lookup_memflags(struct restore_state *rstate);
size_t lookup_memsize(struct restore_state *rstate);
int lookup_guest_ncpus(struct restore_state *rstate);


int restore_vm_mem(struct vmctx *ctx, struct restore_state *rstate);
int restore_kernel_structs(struct vmctx *ctx, struct restore_state *rstate);

int receive_vm_migration(struct vmctx *ctx, char *migration_data);

int restore_pci_devs(struct vmctx *ctx, struct restore_state *rstate);

int get_checkpoint_msg(int conn_fd, struct vmctx *ctx);
void *checkpoint_thread(void *param);
int init_checkpoint_thread(struct vmctx *ctx);


int load_restore_file(const char *filename, struct restore_state *rstate);

#endif
