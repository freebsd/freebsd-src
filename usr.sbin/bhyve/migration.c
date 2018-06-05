
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#ifndef WITHOUT_CAPSICUM
#include <sys/capsicum.h>
#endif
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <machine/atomic.h>
#include <machine/segments.h>

#ifndef WITHOUT_CAPSICUM
#include <capsicum_helpers.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <libgen.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <pthread_np.h>
#include <sysexits.h>
#include <stdbool.h>

#include <machine/vmm.h>
#ifndef WITHOUT_CAPSICUM
#include <machine/vmm_dev.h>
#endif
#include <vmmapi.h>

#include "bhyverun.h"
#include "acpi.h"
#include "atkbdc.h"
#include "inout.h"
#include "dbgport.h"
#include "fwctl.h"
#include "ioapic.h"
#include "mem.h"
#include "mevent.h"
#include "mptbl.h"
#include "pci_emul.h"
#include "pci_irq.h"
#include "pci_lpc.h"
#include "smbiostbl.h"
#include "xmsr.h"
#include "spinup_ap.h"
#include "rtc.h"
#include "migration.h"

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/types.h>
#include <fcntl.h>

#include <libxo/xo.h>
#include <ucl.h>
#include <unistd.h>

#include "migration.h"

extern int guest_ncpus;

#define MB		(1024UL * 1024)
#define GB		(1024UL * MB)

#define BHYVE_RUN_DIR "/var/run/bhyve"
#define CHECKPOINT_RUN_DIR BHYVE_RUN_DIR "/checkpoint"
#define MAX_VMNAME 100

#define MAX_MSG_SIZE 1024

#define SNAPSHOT_BUFFER_SIZE (4 * MB)

#define JSON_STRUCT_ARR_KEY		"structs"
#define JSON_PCI_ARR_KEY		"pci_devices"
#define JSON_BASIC_METADATA_KEY 	"basic metadata"
#define JSON_SNAPSHOT_REQ_KEY		"snapshot_req"
#define JSON_SIZE_KEY			"size"
#define JSON_FILE_OFFSET_KEY		"file_offset"

#define JSON_NCPUS_KEY			"ncpus"
#define JSON_VMNAME_KEY 		"vmname"
#define JSON_MEMSIZE_KEY		"memsize"
#define JSON_MEMFLAGS_KEY		"memflags"

/* TODO: Harden this function and all of its callers since 'base_str' is a user
 * provided string.
 */
static char *
strcat_extension(const char *base_str, const char *ext)
{
	char *res;
	size_t base_len, ext_len;

	base_len = strnlen(base_str, MAX_VMNAME);
	ext_len = strnlen(ext, MAX_VMNAME);

	if (base_len + ext_len > MAX_VMNAME) {
		fprintf(stderr, "Filename exceeds maximum length.\n");
		return (NULL);
	}

	res = malloc(base_len + ext_len + 1);
	if (res == NULL) {
		perror("Failed to allocate memory.");
		return (NULL);
	}

	memcpy(res, base_str, base_len);
	memcpy(res + base_len, ext, ext_len);
	res[base_len + ext_len] = 0;

	return (res);
}

void
destroy_restore_state(struct restore_state *rstate)
{
	if (rstate == NULL) {
		fprintf(stderr, "Attempting to destroy NULL restore struct.\n");
		return;
	}

	if (rstate->kdata_map != MAP_FAILED)
		munmap(rstate->kdata_map, rstate->kdata_len);

	if (rstate->kdata_fd > 0)
		close(rstate->kdata_fd);
	if (rstate->vmmem_fd > 0)
		close(rstate->vmmem_fd);

	if (rstate->meta_root_obj != NULL)
		ucl_object_unref(rstate->meta_root_obj);
	if (rstate->meta_parser != NULL)
		ucl_parser_free(rstate->meta_parser);
}

static int
load_vmmem_file(const char *filename, struct restore_state *rstate)
{
	struct stat sb;
	int err;

	rstate->vmmem_fd = open(filename, O_RDONLY);
	if (rstate->vmmem_fd < 0) {
		perror("Failed to open restore file");
		return (-1);
	}

	err = fstat(rstate->vmmem_fd, &sb);
	if (err < 0) {
		perror("Failed to stat restore file");
		goto err_load_vmmem;
	}

	if (sb.st_size == 0) {
		fprintf(stderr, "Restore file is empty.\n");
		goto err_load_vmmem;
	}

	rstate->vmmem_len = sb.st_size;

	return (0);

err_load_vmmem:
	if (rstate->vmmem_fd > 0)
		close(rstate->vmmem_fd);
	return (-1);
}

static int
load_kdata_file(const char *filename, struct restore_state *rstate)
{
	struct stat sb;
	int err;

	rstate->kdata_fd = open(filename, O_RDONLY);
	if (rstate->kdata_fd < 0) {
		perror("Failed to open kernel data file");
		return (-1);
	}

	err = fstat(rstate->kdata_fd, &sb);
	if (err < 0) {
		perror("Failed to stat kernel data file");
		goto err_load_kdata;
	}

	if (sb.st_size == 0) {
		fprintf(stderr, "Kernel data file is empty.\n");
		goto err_load_kdata;
	}

	rstate->kdata_len = sb.st_size;
	rstate->kdata_map = mmap(NULL, rstate->kdata_len, PROT_READ,
				 MAP_SHARED, rstate->kdata_fd, 0);
	if (rstate->kdata_map == MAP_FAILED) {
		perror("Failed to map restore file");
		goto err_load_kdata;
	}

	return (0);

err_load_kdata:
	if (rstate->kdata_fd > 0)
		close(rstate->kdata_fd);
	return (-1);
}

static int
load_metadata_file(const char *filename, struct restore_state *rstate)
{
	const ucl_object_t *obj;
	struct ucl_parser *parser;
	int err;

	parser = ucl_parser_new(UCL_PARSER_DEFAULT);
	if (parser == NULL) {
		fprintf(stderr, "Failed to initialize UCL parser.\n");
		goto err_load_metadata;
	}

	err = ucl_parser_add_file(parser, filename);
	if (err == 0) {
		fprintf(stderr, "Failed to parse metadata file: '%s'\n",
			filename);
		err = -1;
		goto err_load_metadata;
	}

	obj = ucl_parser_get_object(parser);
	if (obj == NULL) {
		fprintf(stderr, "Failed to parse object.\n");
		err = -1;
		goto err_load_metadata;
	}

	rstate->meta_parser = parser;
	rstate->meta_root_obj = (ucl_object_t *)obj;

	return (0);

err_load_metadata:
	if (parser != NULL)
		ucl_parser_free(parser);
	return (err);
}

int
load_restore_file(const char *filename, struct restore_state *rstate)
{
	int err = 0;
	char *kdata_filename = NULL, *meta_filename = NULL;

	assert(filename != NULL);
	assert(rstate != NULL);

	memset(rstate, 0, sizeof(*rstate));
	rstate->kdata_map = MAP_FAILED;

	err = load_vmmem_file(filename, rstate);
	if (err != 0) {
		fprintf(stderr, "Failed to load guest RAM file.\n");
		goto err_restore;
	}

	kdata_filename = strcat_extension(filename, ".kern");
	if (kdata_filename == NULL) {
		fprintf(stderr, "Failed to construct kernel data filename.\n");
		goto err_restore;
	}

	err = load_kdata_file(kdata_filename, rstate);
	if (err != 0) {
		fprintf(stderr, "Failed to load guest kernel data file.\n");
		goto err_restore;
	}

	meta_filename = strcat_extension(filename, ".meta");
	if (meta_filename == NULL) {
		fprintf(stderr, "Failed to construct kernel metadata filename.\n");
		goto err_restore;
	}

	err = load_metadata_file(meta_filename, rstate);
	if (err != 0) {
		fprintf(stderr, "Failed to load guest metadata file.\n");
		goto err_restore;
	}

	return (0);

err_restore:
	destroy_restore_state(rstate);
	if (kdata_filename != NULL)
		free(kdata_filename);
	if (meta_filename != NULL)
		free(meta_filename);
	return (-1);
}

#define JSON_GET_INT_OR_RETURN(key, obj, result_ptr, ret)			\
do {										\
	const ucl_object_t *obj__;						\
	obj__ = ucl_object_lookup(obj, key);					\
	if (obj__ == NULL) {							\
		fprintf(stderr, "Missing key: '%s'", key);			\
		return (ret);							\
	}									\
	if (!ucl_object_toint_safe(obj__, result_ptr)) {			\
		fprintf(stderr, "Cannot convert '%s' value to int.", key);	\
		return (ret);							\
	}									\
} while(0)

#define JSON_GET_STRING_OR_RETURN(key, obj, result_ptr, ret)			\
do {										\
	const ucl_object_t *obj__;						\
	obj__ = ucl_object_lookup(obj, key);					\
	if (obj__ == NULL) {							\
		fprintf(stderr, "Missing key: '%s'", key);			\
		return (ret);							\
	}									\
	if (!ucl_object_tostring_safe(obj__, result_ptr)) {			\
		fprintf(stderr, "Cannot convert '%s' value to string.", key);	\
		return (ret);							\
	}									\
} while(0)

static void *
lookup_struct(enum snapshot_req struct_id, struct restore_state *rstate,
	      size_t *struct_size)
{
	const ucl_object_t *structs = NULL, *obj = NULL;
	ucl_object_iter_t it = NULL;
	int64_t snapshot_req, size, file_offset;

	structs = ucl_object_lookup(rstate->meta_root_obj,
				    JSON_STRUCT_ARR_KEY);
	if (structs == NULL) {
		fprintf(stderr, "Failed to find '%s' object.\n",
			JSON_STRUCT_ARR_KEY);
		return (NULL);
	}

	if (ucl_object_type((ucl_object_t *)structs) != UCL_ARRAY) {
		fprintf(stderr, "Object '%s' is not an array.\n",
		JSON_STRUCT_ARR_KEY);
		return (NULL);
	}

	while ((obj = ucl_object_iterate(structs, &it, true)) != NULL) {
		snapshot_req = -1;
		JSON_GET_INT_OR_RETURN(JSON_SNAPSHOT_REQ_KEY, obj,
				       &snapshot_req, NULL);
		assert(snapshot_req >= 0);
		if ((enum snapshot_req) snapshot_req == struct_id) {
			JSON_GET_INT_OR_RETURN(JSON_SIZE_KEY, obj,
					       &size, NULL);
			assert(size >= 0);

			JSON_GET_INT_OR_RETURN(JSON_FILE_OFFSET_KEY, obj,
					       &file_offset, NULL);
			assert(file_offset >= 0);
			assert(file_offset + size <= rstate->kdata_len);

			*struct_size = (size_t)size;
			return (rstate->kdata_map + file_offset);
		}
	}

	return (NULL);
}

static void*
lookup_pci_dev(const char *dev_name, struct restore_state *rstate,
	       size_t *data_size)
{
	const ucl_object_t *devs = NULL, *obj = NULL;
	ucl_object_iter_t it = NULL;
	int64_t size, file_offset;
	const char *snapshot_req;

	devs = ucl_object_lookup(rstate->meta_root_obj,
				    JSON_PCI_ARR_KEY);
	if (devs == NULL) {
		fprintf(stderr, "Failed to find '%s' object.\n",
			JSON_PCI_ARR_KEY);
		return (NULL);
	}

	if (ucl_object_type((ucl_object_t *)devs) != UCL_ARRAY) {
		fprintf(stderr, "Object '%s' is not an array.\n",
		JSON_PCI_ARR_KEY);
		return (NULL);
	}

	while ((obj = ucl_object_iterate(devs, &it, true)) != NULL) {
		snapshot_req = NULL;
		JSON_GET_STRING_OR_RETURN(JSON_SNAPSHOT_REQ_KEY, obj,
				       &snapshot_req, NULL);
		assert(snapshot_req != NULL);
		if (!strcmp(snapshot_req, dev_name)) {
			JSON_GET_INT_OR_RETURN(JSON_SIZE_KEY, obj,
					       &size, NULL);
			assert(size >= 0);

			JSON_GET_INT_OR_RETURN(JSON_FILE_OFFSET_KEY, obj,
					       &file_offset, NULL);
			assert(file_offset >= 0);
			assert(file_offset + size <= rstate->kdata_len);

			*data_size = (size_t)size;
			return (rstate->kdata_map + file_offset);
		}
	}

	return (NULL);
}

static const ucl_object_t *
lookup_basic_metadata_object(struct restore_state *rstate)
{
	const ucl_object_t *basic_meta_obj = NULL;

	basic_meta_obj = ucl_object_lookup(rstate->meta_root_obj,
					   JSON_BASIC_METADATA_KEY);
	if (basic_meta_obj == NULL) {
		fprintf(stderr, "Failed to find '%s' object.\n",
			JSON_BASIC_METADATA_KEY);
		return (NULL);
	}

	if (ucl_object_type((ucl_object_t *)basic_meta_obj) != UCL_OBJECT) {
		fprintf(stderr, "Object '%s' is not a JSON object.\n",
		JSON_BASIC_METADATA_KEY);
		return (NULL);
	}

	return (basic_meta_obj);
}

const char *
lookup_vmname(struct restore_state *rstate)
{
	const char *vmname;
	const ucl_object_t *obj;

	obj = lookup_basic_metadata_object(rstate);
	if (obj == NULL)
		return (NULL);

	JSON_GET_STRING_OR_RETURN(JSON_VMNAME_KEY, obj, &vmname, NULL);
	return (vmname);
}

int
lookup_memflags(struct restore_state *rstate)
{
	int64_t memflags;
	const ucl_object_t *obj;

	obj = lookup_basic_metadata_object(rstate);
	if (obj == NULL)
		return (0);

	JSON_GET_INT_OR_RETURN(JSON_MEMFLAGS_KEY, obj, &memflags, 0);

	return ((int)memflags);
}

size_t
lookup_memsize(struct restore_state *rstate)
{
	int64_t memsize;
	const ucl_object_t *obj;

	obj = lookup_basic_metadata_object(rstate);
	if (obj == NULL)
		return (0);

	JSON_GET_INT_OR_RETURN(JSON_MEMSIZE_KEY, obj, &memsize, 0);
	if (memsize < 0)
		memsize = 0;

	return ((size_t)memsize);
}


int
lookup_guest_ncpus(struct restore_state *rstate)
{
	int64_t ncpus;
	const ucl_object_t *obj;

	obj = lookup_basic_metadata_object(rstate);
	if (obj == NULL)
		return (0);

	JSON_GET_INT_OR_RETURN(JSON_NCPUS_KEY, obj, &ncpus, 0);
	return ((int)ncpus);
}

int
restore_vm_mem(struct vmctx *ctx, struct restore_state *rstate)
{
	return vm_restore_mem(ctx, rstate->vmmem_fd, rstate->vmmem_len);
}

int
restore_kernel_structs(struct vmctx *ctx, struct restore_state *rstate)
{
	void *struct_ptr;
	size_t struct_size;
	int ret;
	int i;
	enum snapshot_req structs[] = {
		STRUCT_VMX,
		STRUCT_VM,
		STRUCT_VLAPIC,
		STRUCT_LAPIC,
		STRUCT_VIOAPIC,
		STRUCT_VHPET,
		STRUCT_VMCX,
		STRUCT_VATPIC,
		STRUCT_VATPIT,
		STRUCT_VPMTMR,
		STRUCT_VRTC,
	};

	for (i = 0; i < sizeof(structs)/sizeof(structs[0]); i++) {
		struct_ptr = lookup_struct(structs[i], rstate, &struct_size);
		if (struct_ptr == NULL) {
			fprintf(stderr, "Failed to lookup struct vmx\n");
			return (-1);
		}

		ret = vm_restore_req(ctx, structs[i], struct_ptr, struct_size);
		if (ret != 0) {
			fprintf(stderr, "Failed to restore struct: %d\n", structs[i]);
		}
	}

	return 0;
}

int
receive_vm_migration(struct vmctx *ctx, char *migration_data)
{
	struct migrate_req req;
	char *hostname, *pos;
	int rc;

	memset(req.host, 0, MAX_HOSTNAME_LEN);
	hostname = strdup(migration_data);

	if ((pos = strchr(hostname, ',')) != NULL ) {
		*pos = '\0';
		strncpy(req.host, hostname, MAX_HOSTNAME_LEN);
		pos = pos + 1;

		rc = sscanf(pos, "%d", &(req.port));

		if (rc == 0) {
			fprintf(stderr, "Could not parse the port\r\n");
			free(hostname);
			return -1;
		}
	} else {
		strncpy(req.host, hostname, MAX_HOSTNAME_LEN);

		/* If only one variable could be read, it should be the host */
		req.port = DEFAULT_MIGRATION_PORT;
	}

	rc = vm_recv_migrate_req(ctx, req, pci_restore);

	free(hostname);
	return (rc);
}

int
restore_pci_devs(struct vmctx *ctx, struct restore_state *rstate)
{
	void *dev_ptr;
	size_t dev_size;
	int ret;
	int i;
	char *devs[] = {
		"virtio-net",
		"virtio-blk",
		"lpc",
	};

	for (i = 0; i < sizeof(devs)/sizeof(devs[0]); i++) {
		dev_ptr = lookup_pci_dev(devs[i], rstate, &dev_size);
		if (dev_ptr == NULL) {
			fprintf(stderr, "Failed to lookup dev: %s\n", devs[i]);
			return (-1);
		}

		ret = pci_restore(ctx, devs[i], dev_ptr, dev_size);
		if (ret != 0) {
			fprintf(stderr, "Failed to restore dev: %s\n", devs[i]);
			return (-1);
		}
	}

	return (0);
}


static int
vm_snapshot_kern_data(struct vmctx *ctx, int data_fd, xo_handle_t *xop)
{
	int ret, i, error = 0;
	size_t data_size, offset = 0;
	char *buffer = NULL;
	enum snapshot_req structs[] = {
		STRUCT_VM,
		STRUCT_VMX,
		STRUCT_VIOAPIC,
		STRUCT_VLAPIC,
		STRUCT_LAPIC,
		STRUCT_VHPET,
		STRUCT_VMCX,
		STRUCT_VATPIC,
		STRUCT_VATPIT,
		STRUCT_VPMTMR,
		STRUCT_VRTC,
	};

	char *snapshot_struct_names[] = {"vm", "vmx", "vioapic", "vlapic", "lapic", "vhpet", "vmcs", "vatpic", "vatpit", "vpmtmr", "vrtc"};

	buffer = malloc(SNAPSHOT_BUFFER_SIZE * sizeof(char));
	if (buffer == NULL) {
		perror("Failed to allocate memory for snapshot buffer");
		goto err_vm_snapshot_kern_data;
	}

	xo_open_list_h(xop, JSON_STRUCT_ARR_KEY);
	for (i = 0; i < sizeof(structs) / sizeof(structs[0]); i++) {
		memset(buffer, 0, SNAPSHOT_BUFFER_SIZE);
		ret = vm_snapshot_req(ctx, structs[i], buffer, SNAPSHOT_BUFFER_SIZE,
				&data_size);

		if (ret != 0) {
			fprintf(stderr, "Failed to snapshot struct %s; ret=%d\n",
				snapshot_struct_names[i], ret);
			error = -1;
			goto err_vm_snapshot_kern_data;
		}

		ret = write(data_fd, buffer, data_size);
		if (ret != data_size) {
			perror("Failed to write all snapshotted data.");
			error = -1;
			goto err_vm_snapshot_kern_data;
		}

		/* Write metadata. */
		xo_open_instance_h(xop, JSON_STRUCT_ARR_KEY);
		xo_emit_h(xop, "{:debug_name/%s}\n", snapshot_struct_names[i]);
		xo_emit_h(xop, "{:" JSON_SNAPSHOT_REQ_KEY "/%d}\n", structs[i]);
		xo_emit_h(xop, "{:" JSON_SIZE_KEY "/%lu}\n", data_size);
		xo_emit_h(xop, "{:" JSON_FILE_OFFSET_KEY "/%lu}\n", offset);
		xo_close_instance_h(xop, JSON_STRUCT_ARR_KEY);

		offset += data_size;
	}
	xo_close_list_h(xop, JSON_STRUCT_ARR_KEY);

err_vm_snapshot_kern_data:
	if (buffer != NULL)
		free(buffer);
	return (error);
}

static int
vm_snapshot_basic_metadata(struct vmctx *ctx, xo_handle_t *xop)
{
	size_t memsize;
	int memflags;
	char vmname_buf[MAX_VMNAME];

	memset(vmname_buf, 0, MAX_VMNAME);
	vm_get_name(ctx, vmname_buf, MAX_VMNAME - 1);

	memsize = vm_get_lowmem_size(ctx) + vm_get_highmem_size(ctx);
	memflags = vm_get_memflags(ctx);

	xo_open_container_h(xop, JSON_BASIC_METADATA_KEY);
	xo_emit_h(xop, "{:" JSON_NCPUS_KEY "/%ld}\n", guest_ncpus);
	xo_emit_h(xop, "{:" JSON_VMNAME_KEY "/%s}\n", vmname_buf);
	xo_emit_h(xop, "{:" JSON_MEMSIZE_KEY "/%lu}\n", memsize);
	xo_emit_h(xop, "{:" JSON_MEMFLAGS_KEY "/%d}\n", memflags);
	xo_close_container_h(xop, JSON_BASIC_METADATA_KEY);

	return 0;
}

static int
vm_snapshot_pci_data(struct vmctx *ctx, int data_fd, xo_handle_t *xop)
{
	int ret, i, error = 0;
	size_t data_size;
	off_t offset;
	void *buffer = NULL;
	char *devs[] = {
		"virtio-net",
		"virtio-blk",
		"lpc",
	};

	offset = lseek(data_fd, 0, SEEK_CUR);
	if (offset < 0) {
		perror("Failed to get data file current offset.");
		return (-1);
	}

	buffer = malloc(SNAPSHOT_BUFFER_SIZE);
	if (buffer == NULL) {
		perror("Failed to allocate memory for snapshot buffer");
		goto err_pci;
	}

	xo_open_list_h(xop, JSON_PCI_ARR_KEY);
	for (i = 0; i < sizeof(devs) / sizeof(devs[0]); i++) {
		memset(buffer, 0, SNAPSHOT_BUFFER_SIZE);
		ret = pci_snapshot(ctx, devs[i], buffer, SNAPSHOT_BUFFER_SIZE,
				   &data_size);

		if (ret != 0) {
			fprintf(stderr, "Failed to snapshot pci dev %s; ret=%d\n",
				devs[i], ret);
			error = -1;
			goto err_pci;
		}

		assert(data_size < SNAPSHOT_BUFFER_SIZE);

		ret = write(data_fd, buffer, data_size);
		if (ret != data_size) {
			perror("Failed to write all snapshotted data.");
			error = -1;
			goto err_pci;
		}

		/* Write metadata. */
		xo_open_instance_h(xop, JSON_PCI_ARR_KEY);
		xo_emit_h(xop, "{:" JSON_SNAPSHOT_REQ_KEY "/%s}\n", devs[i]);
		xo_emit_h(xop, "{:" JSON_SIZE_KEY "/%lu}\n", data_size);
		xo_emit_h(xop, "{:" JSON_FILE_OFFSET_KEY "/%lu}\n", offset);
		xo_close_instance_h(xop, JSON_PCI_ARR_KEY);

		offset += data_size;
	}
	xo_close_list_h(xop, JSON_PCI_ARR_KEY);

err_pci:
	if (buffer != NULL)
		free(buffer);
	return (error);
}

static int
vm_mem_write_to_file(int fd, const void *src, size_t dst_offset, size_t len)
{
	size_t write_total;
	ssize_t cnt_write;
	size_t to_write;

	write_total = 0;
	to_write = len;

	if (lseek(fd, dst_offset, SEEK_SET) < 0 ) {
		perror("Failed to changed file offset");
		return (-1);
	}

	while (write_total < len) {
		cnt_write = write(fd, src + write_total, to_write);
		if (cnt_write < 0) {
			perror("Failed to write in file");
			return (-1);
		}
		to_write -= cnt_write;
		write_total += cnt_write;
	}

	return (0);
}

static int
vm_checkpoint(struct vmctx *ctx, char *checkpoint_file, bool stop_vm)
{
	int fd_checkpoint = 0, kdata_fd = 0;
	int ret = 0;
	int error = 0;
	char *mmap_vm_lowmem = MAP_FAILED;
	char *mmap_vm_highmem = MAP_FAILED;
	char *mmap_checkpoint_file = MAP_FAILED;
	size_t guest_lowmem, guest_highmem, guest_memsize;
	char *guest_baseaddr;
	xo_handle_t *xop = NULL;
	char *meta_filename = NULL;
	char *kdata_filename = NULL;
	FILE *meta_file = NULL;

	kdata_filename = strcat_extension(checkpoint_file, ".kern");
	if (kdata_filename == NULL) {
		fprintf(stderr, "Failed to construct kernel data filename.\n");
		return (-1);
	}

	kdata_fd = open(kdata_filename, O_WRONLY | O_CREAT | O_TRUNC, 0700);
	if (kdata_fd < 0) {
		perror("Failed to open kernel data snapshot file.");
		error = -1;
		goto done;
	}

	fd_checkpoint = open(checkpoint_file, O_RDWR | O_CREAT | O_TRUNC, 0700);

	if (fd_checkpoint < 0) {
		perror("Failed to create checkpoint file");
		error = -1;
		goto done;
	}

	ret = vm_get_guestmem_from_ctx(ctx, &guest_baseaddr, &guest_lowmem, &guest_highmem);
	guest_memsize = guest_lowmem + guest_highmem;
	if (ret < 0) {
		fprintf(stderr, "Failed to get guest mem information (base, low, high)\n");
		error = -1;
		goto done;
	}

	/* make space for VMs address space */
	ret = ftruncate(fd_checkpoint, guest_memsize);
	if (ret < 0) {
		perror("Failed to truncate checkpoint file\n");
		goto done;
	}

	meta_filename = strcat_extension(checkpoint_file, ".meta");
	if (meta_filename == NULL) {
		fprintf(stderr, "Failed to construct vm metadata filename.\n");
		goto done;
	}

	meta_file = fopen(meta_filename, "w");
	if (meta_file == NULL) {
		perror("Failed to open vm metadata snapshot file.");
		goto done;
	}

	xop = xo_create_to_file(meta_file, XO_STYLE_JSON, XOF_PRETTY);
	if (xop == NULL) {
		perror("Failed to get libxo handle on metadata file.");
		goto done;
	}

	ret = vm_snapshot_basic_metadata(ctx, xop);
	if (ret != 0) {
		fprintf(stderr, "Failed to snapshot vm basic metadata.\n");
		error = -1;
		goto done;
	}

	/*
	 * mmap VMs memory in bhyverun virtual memory: the original address space
	 * (of the VM) will be COW
	 */
	ret = vm_get_vm_mem(ctx, &mmap_vm_lowmem, &mmap_vm_highmem,
			guest_baseaddr, guest_lowmem, guest_highmem);
	if (ret != 0) {
		fprintf(stderr, "Could not mmap guests lowmem and highmem\n");
		error = ret;
		goto done;
	}

	vm_vcpu_lock_all(ctx);

	/*
	 * mmap checkpoint file in memory so we can easily copy VMs
	 * system address space (lowmem + highmem) from kernel space
	 */
	if (vm_mem_write_to_file(fd_checkpoint, mmap_vm_lowmem,
				0, guest_lowmem) != 0) {
		perror("Could not write lowmem");
		error = -1;
		goto done_unlock;
	}

	if (guest_highmem > 0) {
		if (vm_mem_write_to_file(fd_checkpoint, mmap_vm_highmem,
				guest_lowmem, guest_highmem) != 0) {
			perror("Could not write highmem");
			error = -1;
			goto done_unlock;
		}
	}

	ret = vm_snapshot_kern_data(ctx, kdata_fd, xop);
	if (ret != 0) {
		fprintf(stderr, "Failed to snapshot vm kernel data.\n");
		error = -1;
		goto done_unlock;
	}

	ret = vm_snapshot_pci_data(ctx, kdata_fd, xop);
	if (ret != 0) {
		fprintf(stderr, "Failed to snapshot PCI state.\n");
		error = -1;
		goto done_unlock;
	}

	xo_finish_h(xop);

	if (stop_vm) {
		ret = vm_suspend(ctx, VM_SUSPEND_POWEROFF);
		if (ret != 0) {
			fprintf(stderr, "Failed to suspend vm\n");
		}
		vm_vcpu_unlock_all(ctx);
		/* Wait for CPUs to suspend. TODO: write this properly. */
		sleep(5);
		vm_destroy(ctx);
		exit(0);
	}

done_unlock:
	vm_vcpu_unlock_all(ctx);
done:
	if (fd_checkpoint > 0)
		close(fd_checkpoint);
	if (mmap_checkpoint_file != MAP_FAILED)
		munmap(mmap_checkpoint_file, guest_memsize);
	if (mmap_vm_lowmem != MAP_FAILED)
		munmap(mmap_vm_lowmem, guest_lowmem);
	if (mmap_vm_highmem != MAP_FAILED)
		munmap(mmap_vm_highmem, guest_highmem);
	if (meta_filename != NULL)
		free(meta_filename);
	if (kdata_filename != NULL)
		free(kdata_filename);
	if (xop != NULL)
		xo_destroy(xop);
	if (meta_file != NULL)
		fclose(meta_file);
	if (kdata_fd > 0)
		close(kdata_fd);
	return (error);
}

int get_checkpoint_msg(int conn_fd, struct vmctx *ctx)
{
	unsigned char buf[MAX_MSG_SIZE];
	struct checkpoint_op *checkpoint_op;
	struct migrate_req req;
	int len, recv_len, total_recv = 0;
	int err = 0;

	len = sizeof(struct checkpoint_op); /* expected length */
	while ((recv_len = recv(conn_fd, buf + total_recv, len - total_recv, 0)) > 0) {
		total_recv += recv_len;
	}
	if (recv_len < 0) {
		perror("Error while receiving data from bhyvectl");
		err = -1;
		goto done;
	}

	checkpoint_op = (struct checkpoint_op *)buf;
	switch (checkpoint_op->op) {
		case START_CHECKPOINT:
			err = vm_checkpoint(ctx, checkpoint_op->snapshot_filename, false);
			break;
		case START_SUSPEND:
			err = vm_checkpoint(ctx, checkpoint_op->snapshot_filename, true);
			break;
		case START_MIGRATE:
			memset(&req, 0, sizeof(struct migrate_req));
			req.port = checkpoint_op->port;
			memcpy(req.host, checkpoint_op->host, MAX_HOSTNAME_LEN);
			req.host[MAX_HOSTNAME_LEN - 1] = 0;
			fprintf(stderr, "%s: IP address used for migration: %s;\r\n"
				"Port used for migration: %d\r\n",
				__func__,
				checkpoint_op->host,
				checkpoint_op->port);

			err = vm_send_migrate_req(ctx, req, pci_snapshot);
			break;
		default:
			fprintf(stderr, "Unrecognized checkpoint operation.\n");
			err = -1;
	}

done:
	close(conn_fd);
	return (err);
}

/*
 * Listen for commands from bhyvectl
 */
void * checkpoint_thread(void *param)
{
	struct checkpoint_thread_info *thread_info;
	socklen_t addr_len;
	int conn_fd, ret;

	thread_info = (struct checkpoint_thread_info *)param;

	addr_len = sizeof(thread_info->addr);
	while ((conn_fd = accept(thread_info->socket_fd,
			(struct sockaddr *) thread_info->addr,
			&addr_len)) > -1) {
		ret = get_checkpoint_msg(conn_fd, thread_info->ctx);
		if (ret != 0) {
			fprintf(stderr, "Failed to read message on checkpoint "
					"socket. Retrying.\n");
		}

		addr_len = sizeof(struct sockaddr_un);
	}
	if (conn_fd < -1) {
		perror("Failed to accept connection");
	}

	return (NULL);
}

/*
 * Create directory tree to store runtime specific information:
 * i.e. UNIX sockets for IPC with bhyvectl.
 */
static int
make_checkpoint_dir()
{
	int err;

	err = mkdir(BHYVE_RUN_DIR, 0755);
	if (err < 0 && errno != EEXIST)
		return (err);

	err = mkdir(CHECKPOINT_RUN_DIR, 0755);
	if (err < 0 && errno != EEXIST)
		return (err);

	return 0;
}

/*
 * Create the listening socket for IPC with bhyvectl
 */
int init_checkpoint_thread(struct vmctx *ctx)
{
	struct sockaddr_un addr;
	int socket_fd;
	pthread_t checkpoint_pthread;
	char vmname_buf[MAX_VMNAME];
	int ret, err = 0;

	socket_fd = socket(PF_UNIX, SOCK_STREAM, 0);
	if (socket_fd < 0) {
		perror("Socket creation failed (IPC with bhyvectl");
		err = -1;
		goto fail;
	}

	err = make_checkpoint_dir();
	if (err < 0) {
		perror("Failed to create checkpoint runtime directory");
		goto fail;
	}

	memset(&addr, 0, sizeof(struct sockaddr_un));
	addr.sun_family = AF_UNIX;
	vm_get_name(ctx, vmname_buf, MAX_VMNAME - 1);
	snprintf(addr.sun_path, PATH_MAX, "%s/%s",
		 CHECKPOINT_RUN_DIR, vmname_buf);
	unlink(addr.sun_path);

	if (bind(socket_fd, (struct sockaddr *)&addr,
			sizeof(struct sockaddr_un)) != 0) {
		perror("Failed to bind socket (IPC with bhyvectl)");
		err = -1;
		goto fail;
	}

	if (listen(socket_fd, 10) < 0) {
		perror("Failed to listen on socket (IPC with bhyvectl)");
		err = -1;
		goto fail;
	}

	memset(&checkpoint_info, 0, sizeof(struct checkpoint_thread_info));
	checkpoint_info.ctx = ctx;
	checkpoint_info.socket_fd = socket_fd;
	checkpoint_info.addr = &addr;


	/* TODO: start thread for listening connections */
	pthread_set_name_np(checkpoint_pthread, "checkpoint thread");
	ret = pthread_create(&checkpoint_pthread, NULL, checkpoint_thread,
		&checkpoint_info);
	if (ret < 0) {
		err = ret;
		goto fail;
	}

	return (0);
fail:
	if (socket_fd > 0)
		close(socket_fd);
	unlink(addr.sun_path);

	return (err);
}

