
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#ifndef WITHOUT_CAPSICUM
#include <sys/capsicum.h>
#endif
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/param.h>

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
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>

#include <sys/sysctl.h>

#include <libxo/xo.h>
#include <ucl.h>
#include <unistd.h>

extern int guest_ncpus;

#define MB		(1024UL * 1024)
#define GB		(1024UL * MB)

#define BHYVE_RUN_DIR "/var/run/bhyve"
#define CHECKPOINT_RUN_DIR BHYVE_RUN_DIR "/checkpoint"
#define MAX_VMNAME 100

#define MAX_MSG_SIZE 1024

#define SNAPSHOT_BUFFER_SIZE (20 * MB)

#define JSON_STRUCT_ARR_KEY		"structs"
#define JSON_DEV_ARR_KEY		"devices"
#define JSON_BASIC_METADATA_KEY 	"basic metadata"
#define JSON_SNAPSHOT_REQ_KEY		"snapshot_req"
#define JSON_SIZE_KEY			"size"
#define JSON_FILE_OFFSET_KEY		"file_offset"

#define JSON_NCPUS_KEY			"ncpus"
#define JSON_VMNAME_KEY 		"vmname"
#define JSON_MEMSIZE_KEY		"memsize"
#define JSON_MEMFLAGS_KEY		"memflags"

const struct vm_snapshot_dev_info snapshot_devs[] = {
	{
		.dev_name = "atkbdc",
		.snapshot_cb = atkbdc_snapshot,
		.restore_cb = atkbdc_restore
	},
	{
		.dev_name = "virtio-net",
		.snapshot_cb = pci_snapshot,
		.restore_cb = pci_restore
	},
	{
		.dev_name = "virtio-blk",
		.snapshot_cb = pci_snapshot,
		.restore_cb = pci_restore
	},
	{
		.dev_name = "lpc",
		.snapshot_cb = pci_snapshot,
		.restore_cb = pci_restore
	},
	{
		.dev_name = "fbuf",
		.snapshot_cb = pci_snapshot,
		.restore_cb = pci_restore
	},
	{
		.dev_name = "xhci",
		.snapshot_cb = pci_snapshot,
		.restore_cb = pci_restore
	},
	{
		.dev_name = "e1000",
		.snapshot_cb = pci_snapshot,
		.restore_cb = pci_restore
	},
};

const struct vm_snapshot_kern_info snapshot_kern_structs[] = {
	{
		.struct_name = "vm",
		.req = STRUCT_VM
	},
	{
		.struct_name = "vmx",
		.req = STRUCT_VMX
	},
	{
		.struct_name = "vioapic",
		.req = STRUCT_VIOAPIC
	},
	{
		.struct_name = "vlapic",
		.req = STRUCT_VLAPIC
	},
	{
		.struct_name = "lapic",
		.req = STRUCT_LAPIC
	},
	{
		.struct_name = "vhpet",
		.req = STRUCT_VHPET
	},
	{
		.struct_name = "vmcx",
		.req = STRUCT_VMCX
	},
	{
		.struct_name = "vatpit",
		.req = STRUCT_VATPIT
	},
	{
		.struct_name = "vatpic",
		.req = STRUCT_VATPIC
	},
	{
		.struct_name = "vpmtmr",
		.req = STRUCT_VPMTMR
	},
	{
		.struct_name = "vrtc",
		.req = STRUCT_VRTC
	},
};

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

	structs = ucl_object_lookup(rstate->meta_root_obj, JSON_STRUCT_ARR_KEY);
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

static void *
lookup_check_dev(const char *dev_name, struct restore_state *rstate,
		 const ucl_object_t *obj, size_t *data_size)
{
	const char *snapshot_req;
	int64_t size, file_offset;

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

	return (NULL);
}

static void*
lookup_dev(const char *dev_name, struct restore_state *rstate,
	   size_t *data_size)
{
	const ucl_object_t *devs = NULL, *obj = NULL;
	ucl_object_iter_t it = NULL;
	void *ret;

	devs = ucl_object_lookup(rstate->meta_root_obj, JSON_DEV_ARR_KEY);
	if (devs == NULL) {
		fprintf(stderr, "Failed to find '%s' object.\n",
			JSON_DEV_ARR_KEY);
		return (NULL);
	}

	if (ucl_object_type((ucl_object_t *)devs) != UCL_ARRAY) {
		fprintf(stderr, "Object '%s' is not an array.\n",
			JSON_DEV_ARR_KEY);
		return (NULL);
	}

	while ((obj = ucl_object_iterate(devs, &it, true)) != NULL) {
		ret = lookup_check_dev(dev_name, rstate, obj, data_size);
		if (ret != NULL)
			return (ret);
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

	for (i = 0; i < nitems(snapshot_kern_structs); i++) {
		struct_ptr = lookup_struct(snapshot_kern_structs[i].req, rstate, &struct_size);
		if (struct_ptr == NULL) {
			fprintf(stderr, "%s: Failed to lookup struct %s\r\n",
				__func__, snapshot_kern_structs[i].struct_name);
			return (-1);
		}

		ret = vm_restore_req(ctx, snapshot_kern_structs[i].req, struct_ptr, struct_size);
		if (ret != 0) {
			fprintf(stderr, "%s: Failed to restore struct: %s\r\n",
				__func__, snapshot_kern_structs[i].struct_name);
			return (ret);
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

	rc = vm_recv_migrate_req(ctx, req);

	free(hostname);
	return (rc);
}

static int
restore_dev(struct vmctx *ctx, struct restore_state *rstate,
	    const struct vm_snapshot_dev_info *info)
{
	void *dev_ptr;
	size_t dev_size;
	int ret;

	dev_ptr = lookup_dev(info->dev_name, rstate, &dev_size);
	if (dev_ptr == NULL) {
		fprintf(stderr, "Failed to lookup dev: %s\r\n", info->dev_name);
		fprintf(stderr, "Continuing the restore/migration process\r\n");
		return (0);
	}

	if (dev_size == 0) {
		fprintf(stderr, "%s: Device size is 0. "
			"Assuming %s is not used\r\n",
			__func__, info->dev_name);
		return (0);
	}

	ret = (*info->restore_cb)(ctx, info->dev_name, dev_ptr, dev_size);
	if (ret != 0) {
		fprintf(stderr, "Failed to restore dev: %s\r\n",
			info->dev_name);
		return (-1);
	}

	return (0);

}


int
restore_devs(struct vmctx *ctx, struct restore_state *rstate)
{
	int ret;
	int i;

	for (i = 0; i < nitems(snapshot_devs); i++) {
		ret = restore_dev(ctx, rstate, &snapshot_devs[i]);
		if (ret != 0)
			return (ret);
	}

	return 0;
}

static int
vm_snapshot_kern_data(struct vmctx *ctx, int data_fd, xo_handle_t *xop)
{
	int ret, i, error = 0;
	size_t data_size, offset = 0;
	char *buffer = NULL;

	buffer = malloc(SNAPSHOT_BUFFER_SIZE * sizeof(char));
	if (buffer == NULL) {
		perror("Failed to allocate memory for snapshot buffer");
		goto err_vm_snapshot_kern_data;
	}

	xo_open_list_h(xop, JSON_STRUCT_ARR_KEY);
	for (i = 0; i < nitems(snapshot_kern_structs); i++) {
		memset(buffer, 0, SNAPSHOT_BUFFER_SIZE);
		data_size = 0;
		ret = vm_snapshot_req(ctx, snapshot_kern_structs[i].req, buffer,
				      SNAPSHOT_BUFFER_SIZE, &data_size);

		if (ret != 0) {
			fprintf(stderr, "%s: Failed to snapshot struct %s; ret=%d\r\n",
				__func__, snapshot_kern_structs[i].struct_name, ret);
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
		xo_emit_h(xop, "{:debug_name/%s}\n", snapshot_kern_structs[i].struct_name);
		xo_emit_h(xop, "{:" JSON_SNAPSHOT_REQ_KEY "/%d}\n",
			snapshot_kern_structs[i].req);
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
vm_snapshot_dev_write_data(int data_fd, void *buffer, size_t data_size,
			   xo_handle_t *xop, const char *array_key,
			   const char *dev_name, off_t offset)
{
	int ret;

	assert(data_size < SNAPSHOT_BUFFER_SIZE);

	ret = write(data_fd, buffer, data_size);
	if (ret != data_size) {
		perror("Failed to write all snapshotted data.");
		return (-1);
	}

	/* Write metadata. */
	xo_open_instance_h(xop, array_key);
	xo_emit_h(xop, "{:" JSON_SNAPSHOT_REQ_KEY "/%s}\n", dev_name);
	xo_emit_h(xop, "{:" JSON_SIZE_KEY "/%lu}\n", data_size);
	xo_emit_h(xop, "{:" JSON_FILE_OFFSET_KEY "/%lu}\n", offset);
	xo_close_instance_h(xop, array_key);

	return (0);
}

static int
vm_snapshot_dev(const struct vm_snapshot_dev_info *info,
		struct vmctx *ctx, int data_fd, xo_handle_t *xop,
		void *buffer, size_t buf_size, off_t *offset)
{
	int ret;
	size_t data_size;

	memset(buffer, 0, buf_size);
	data_size = 0;
	ret = (*info->snapshot_cb)(ctx, info->dev_name, buffer, buf_size,
				   &data_size);
	if (ret != 0) {
		fprintf(stderr, "Failed to snapshot %s; ret=%d\r\n",
			info->dev_name, ret);
		return (ret);
	}

	ret = vm_snapshot_dev_write_data(data_fd, buffer, data_size,
					 xop, JSON_DEV_ARR_KEY, info->dev_name,
					 *offset);
	if (ret != 0)
		return (ret);

	*offset += data_size;

	return (0);
}

static int
vm_snapshot_dev_data(struct vmctx *ctx, int data_fd, xo_handle_t *xop)
{
	int ret, i;
	off_t offset;
	void *buffer = NULL;
	size_t buf_size;

	buf_size = SNAPSHOT_BUFFER_SIZE;

	offset = lseek(data_fd, 0, SEEK_CUR);
	if (offset < 0) {
		perror("Failed to get data file current offset.");
		return (-1);
	}

	buffer = malloc(buf_size);
	if (buffer == NULL) {
		perror("Failed to allocate memory for snapshot buffer");
		ret = ENOSPC;
		goto snapshot_err;
	}

	xo_open_list_h(xop, JSON_DEV_ARR_KEY);

	/* Restore other devices that support this feature */
	for (i = 0; i < nitems(snapshot_devs); i++) {
		ret = vm_snapshot_dev(&snapshot_devs[i], ctx, data_fd, xop,
				      buffer, buf_size, &offset);
		if (ret != 0)
			goto snapshot_err;
	}

	xo_close_list_h(xop, JSON_DEV_ARR_KEY);

snapshot_err:
	if (buffer != NULL)
		free(buffer);
	return (ret);
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

	ret = vm_snapshot_dev_data(ctx, kdata_fd, xop);
	if (ret != 0) {
		fprintf(stderr, "Failed to snapshot device state.\n");
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

			err = vm_send_migrate_req(ctx, req);
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

// Warm Migration

static int
get_system_specs_for_migration(struct migration_system_specs *specs)
{
	int mib[2];
	size_t len_machine, len_model, len_pagesize;
	char interm[MAX_SPEC_LEN];
	int rc;
	int num;

	mib[0] = CTL_HW;

	mib[1] = HW_MACHINE;
	memset(interm, 0, MAX_SPEC_LEN);
	rc = sysctl(mib, 2, interm, &len_machine, NULL, 0);
	if (rc != 0) {
		perror("Could not retrieve HW_MACHINE specs");
		return (rc);
	}
	strncpy(specs->hw_machine, interm, MAX_SPEC_LEN);

	memset(interm, 0, MAX_SPEC_LEN);
	mib[0] = CTL_HW;
	mib[1] = HW_MODEL;
	rc = sysctl(mib, 2, interm, &len_model, NULL, 0);
	if (rc != 0) {
		perror("Could not retrieve HW_MODEL specs");
		return (rc);
	}
	strncpy(specs->hw_model, interm, MAX_SPEC_LEN);

	mib[0] = CTL_HW;
	mib[1] = HW_PAGESIZE;
	rc = sysctl(mib, 2, &num, &len_pagesize, NULL, 0);
	if (rc != 0) {
		perror("Could not retrieve HW_PAGESIZE specs");
		return (rc);
	}
	specs->hw_pagesize = num;

	return (0);
}

static int
migration_send_data_remote(int socket, const void *msg, size_t len)
{
	size_t to_send, total_sent;
	ssize_t sent;

	to_send = len;
	total_sent = 0;

	while (to_send > 0) {
		sent  = send(socket, msg + total_sent, to_send, 0);
		if (sent < 0) {
			perror("Error while sending data");
			return (sent);
		}

		to_send -= sent;
		total_sent += sent;
	}

	return (0);
}

static int
migration_recv_data_from_remote(int socket, void *msg, size_t len)
{
	size_t to_recv, total_recv;
	ssize_t recvt;

	to_recv = len;
	total_recv = 0;

	while (to_recv > 0) {
		recvt = recv(socket, msg + total_recv, to_recv, 0);
		if (recvt <= 0) {
			perror("Error while receiving data");
			return (recvt);
		}

		to_recv -= recvt;
		total_recv += recvt;
	}

	return (0);
}

static int
migration_send_specs(int socket)
{
	struct migration_system_specs local_specs;
	struct migration_message_type mesg;
	size_t response;
	int rc;

	rc = get_system_specs_for_migration(&local_specs);
	if (rc != 0) {
		fprintf(stderr, "%s: Could not retrieve local specs\r\n",
			__func__);
		return (rc);
	}

	// Send message type to server: specs & len
	mesg.type = MESSAGE_TYPE_SPECS;
	mesg.len = sizeof(local_specs);
	rc = migration_send_data_remote(socket, &mesg, sizeof(mesg));
	if (rc < 0) {
		fprintf(stderr, "%s: Could not send message type\r\n", __func__);
		return (-1);
	}

	// Send specs to server
	rc = migration_send_data_remote(socket, &local_specs, sizeof(local_specs));
	if (rc < 0) {
		fprintf(stderr, "%s: Could not send system specs\r\n", __func__);
		return (-1);
	}

	// Recv OK/NOT_OK from server
	rc = migration_recv_data_from_remote(socket, &response, sizeof(response));
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not receive response from server\r\n",
			__func__);
		return (-1);
	}

	//  Return OK/NOT_OK
	if (response == MIGRATION_SPECS_NOT_OK) {
		fprintf(stderr,
			"%s: System specification mismatch\r\n",
			__func__);
		return (-1);
	}

	fprintf(stdout, "%s: System specification accepted\r\n", __func__);

	return (0);
}

static int
migration_recv_and_check_specs(int socket)
{
	struct migration_system_specs local_specs;
	struct migration_system_specs remote_specs;
	struct migration_message_type msg;
	size_t response;
	int rc;

	// TODO1: Get specs size from remote (from client)
	rc = migration_recv_data_from_remote(socket, &msg, sizeof(msg));
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not receive message type for specs from remote\r\n",
			__func__);
		return (rc);
	}

	if (msg.type != MESSAGE_TYPE_SPECS) {
		fprintf(stderr,
			"%s: Wrong message type received from remote\r\n",
			__func__);
		return (-1);
	}

	// Get specs from remote (from client)
	rc = migration_recv_data_from_remote(socket, &remote_specs, msg.len);
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not receive specs from remote\r\n",
			__func__);
		return (rc);
	}

	rc = get_system_specs_for_migration(&local_specs);

	if (rc != 0) {
		fprintf(stderr, "%s: Could not get local specs\r\n", __func__);
		return (rc);
	}

	// Check specs
	response = MIGRATION_SPECS_OK;
	if ((strncmp(local_specs.hw_model, remote_specs.hw_model, MAX_SPEC_LEN) != 0)
		|| (strncmp(local_specs.hw_machine, remote_specs.hw_machine, MAX_SPEC_LEN) != 0)
		|| (local_specs.hw_pagesize  != remote_specs.hw_pagesize)
	   ) {
		fprintf(stderr, "%s: System specification mismatch\r\n", __func__);

		// Debug message
		fprintf(stderr,
			"%s: Local specs vs Remote Specs: \r\n"
			"\tmachine: %s vs %s\r\n"
			"\tmodel: %s vs %s\r\n"
			"\tpagesize: %zu vs %zu\r\n",
			__func__,
			local_specs.hw_machine,
			remote_specs.hw_machine,
			local_specs.hw_model,
			remote_specs.hw_model,
			local_specs.hw_pagesize,
			remote_specs.hw_pagesize
			);
		response = MIGRATION_SPECS_NOT_OK;
	}

	// Send OK/NOT_OK to client
	rc = migration_send_data_remote(socket, &response, sizeof(response));
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not send response to remote\r\n",
			__func__);
		return (-1);
	}

	// If NOT_OK, return NOT_OK
	if (response == MIGRATION_SPECS_NOT_OK)
		return (-1);

	return (0);
}

static int
get_migration_host_and_type(const char *hostname, unsigned char *ipv4_addr,
				unsigned char *ipv6_addr, int *type)
{
	struct addrinfo hints, *res;
	void *addr;
	int rc;

	memset(&hints, 0, sizeof(hints));

	hints.ai_family = AF_UNSPEC;

	rc = getaddrinfo(hostname, NULL, &hints, &res);

	if (rc != 0) {
		fprintf(stderr, "%s: Could not get address info\r\n", __func__);
		return (-1);
	}

	*type = res->ai_family;
	switch(res->ai_family) {
		case AF_INET:
			addr = &((struct sockaddr_in *) res->ai_addr)->sin_addr;
			inet_ntop(res->ai_family, addr, ipv4_addr, MAX_IP_LEN);
			printf("hostname %s\r\n", ipv4_addr);
			break;
		case AF_INET6:
			addr = &((struct sockaddr_in6 *) res->ai_addr)->sin6_addr;
			inet_ntop(res->ai_family, addr, ipv6_addr, MAX_IP_LEN);
			printf("hostname %s\r\n", ipv6_addr);
			break;
		default:
			fprintf(stderr, "%s: Unknown ai_family.\r\n", __func__);
			return (-1);
	}

	return (0);
}

static int
migrate_check_memsize(size_t local_lowmem_size, size_t local_highmem_size,
		      size_t remote_lowmem_size, size_t remote_highmem_size)
{
	int ret = MIGRATION_SPECS_OK;

	if (local_lowmem_size != remote_lowmem_size){
		ret = MIGRATION_SPECS_NOT_OK;
		fprintf(stderr,
			"%s: Local and remote lowmem size mismatch\r\n",
			__func__);
	}

	if (local_highmem_size != remote_highmem_size){
		ret = MIGRATION_SPECS_NOT_OK;
		fprintf(stderr,
			"%s: Local and remote highmem size mismatch\r\n",
			__func__);
	}

	return (ret);
}

static int
migrate_recv_memory(struct vmctx *ctx, int socket)
{
	size_t local_lowmem_size = 0, local_highmem_size = 0;
	size_t remote_lowmem_size = 0, remote_highmem_size = 0;
	char *mmap_vm_lowmem = MAP_FAILED;
	char *mmap_vm_highmem = MAP_FAILED;
	char *baseaddr;
	int memsize_ok;
	char *buffer;
	size_t i, chunks, chunk_size = 4 * MB;
	int rc = 0;

	rc = vm_get_guestmem_from_ctx(ctx,
			&baseaddr, &local_lowmem_size,
			&local_highmem_size);
	if (rc != 0) {
		fprintf(stderr,
			"%s: Could not get guest lowmem size and highmem size\r\n",
			__func__);
		return (rc);
	}

	// recv remote_lowmem_size
	rc = migration_recv_data_from_remote(socket,
			&remote_lowmem_size,
			sizeof(size_t));
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not recv lowmem size\r\n",
			__func__);
		return (rc);
	}
	// recv remote_highmem_size
	rc = migration_recv_data_from_remote(socket,
			&remote_highmem_size,
			sizeof(size_t));
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not recv highmem size\r\n",
			__func__);
		return (rc);
	}
	// check if local low/high mem is equal with remote low/high mem
	memsize_ok = migrate_check_memsize(local_lowmem_size, local_highmem_size,
					   remote_lowmem_size, remote_highmem_size);

	// Send migration_ok to remote
	rc = migration_send_data_remote(socket,
			&memsize_ok, sizeof(memsize_ok));
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not send migration_ok to remote\r\n",
			__func__);
		return (rc);
	}

	if (memsize_ok != MIGRATION_SPECS_OK) {
		fprintf(stderr,
			"%s: Memory size mismatch with remote host\r\n",
			__func__);
		return (-1);
	}

	// map highmem and lowmem
	rc = vm_get_vm_mem(ctx, &mmap_vm_lowmem, &mmap_vm_highmem,
			   baseaddr, local_lowmem_size,
			   local_highmem_size);
	if (rc != 0) {
		fprintf(stderr,
			"%s: Could not mmap guest lowmem and highmem\r\n",
			__func__);
		return (rc);
	}

	buffer = malloc(chunk_size * sizeof(char));
	if (buffer == NULL) {
		fprintf(stderr,
			"%s: Could not allocate memory\r\n",
			__func__);
		return (-1);
	}

	// recv lowmem
	chunks = local_lowmem_size / chunk_size;

	for (i = 0 ; i < chunks; i++) {
		memset(buffer, 0, chunk_size);

		rc = migration_recv_data_from_remote(socket, buffer, chunk_size);
		if (rc < 0) {
			fprintf(stderr,
				"%s: Could not recv chunk %lu\r\n",
				__func__,
				i);
			return (-1);
		}

		memcpy(mmap_vm_lowmem + i * chunk_size, buffer, chunk_size);
	}

	// recv highmem
	if (local_highmem_size > 0 ){
		rc = migration_recv_data_from_remote(socket,
				mmap_vm_highmem,
				local_highmem_size);
		if (rc < 0) {
			fprintf(stderr,
				"%s: Could not recv highmem\r\n",
				__func__);
			return (-1);
		}
	}

	return (0);
}

static int
migrate_send_memory(struct vmctx *ctx, int socket)
{
	size_t lowmem_size, highmem_size;
	char *mmap_vm_lowmem = MAP_FAILED;
	char *mmap_vm_highmem = MAP_FAILED;
	char *baseaddr;
	char *buffer;
	size_t chunks, i;
	size_t chunk_size = 4 * MB;
	int memsize_ok;
	int rc = 0;

	rc = vm_get_guestmem_from_ctx(ctx, &baseaddr,
			&lowmem_size, &highmem_size);
	if (rc != 0) {
		fprintf(stderr,
			"%s: Could not get guest lowmem size and highmem size\r\n",
			__func__);
		return (rc);
	}

	// send lowmem_size
	rc = migration_send_data_remote(socket, &lowmem_size, sizeof(size_t));
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not send lowmem size\r\n",
			__func__);
		return (rc);
	}

	// send highmem_size
	rc = migration_send_data_remote(socket, &highmem_size, sizeof(size_t));
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not send highmem size\r\n",
			__func__);
		return (rc);
	}

	// wait for answer - params ok
	rc = migration_recv_data_from_remote(socket, &memsize_ok, sizeof(memsize_ok));
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not receive response from remote\r\n",
			__func__);
		return (rc);
	}

	if (memsize_ok != MIGRATION_SPECS_OK) {
		fprintf(stderr,
			"%s: Memory size mismatch with remote host\r\n",
			__func__);
		return (-1);
	}

	mmap_vm_lowmem = baseaddr;
	mmap_vm_highmem = baseaddr + 4 * GB;

	// send lowmem
	chunks = lowmem_size / chunk_size;

	buffer = malloc(chunk_size * sizeof(char));
	if (buffer == NULL) {
		fprintf(stderr,
			"%s: Could not allocate memory\r\n",
			__func__);
		return (-1);
	}

	for (i = 0 ; i < chunks; i++) {
		memset(buffer, 0, chunk_size);
		memcpy(buffer, mmap_vm_lowmem + i * chunk_size, chunk_size);

		rc = migration_send_data_remote(socket, buffer, chunk_size);
		if (rc < 0) {
			fprintf(stderr,
				"%s: Could not send chunk %lu\r\n",
				__func__,
				i);
			return (-1);
		}
	}

	// send highmem
	if (highmem_size > 0 ){
		rc = migration_send_data_remote(socket, mmap_vm_highmem, highmem_size);
		if (rc < 0) {
			fprintf(stderr,
				"%s: Could not send highmem\r\n",
				__func__);
			return (-1);
		}
	}

	return (0);
}

static inline int
migrate_send_kern_struct(struct vmctx *ctx, int socket,
			 char *buffer,
			 enum snapshot_req struct_req)
{
	int rc;
	size_t data_size;
	struct migration_message_type msg;

	memset(&msg, 0, sizeof(msg));

	msg.type = MESSAGE_TYPE_KERN;
	rc = vm_snapshot_req(ctx, struct_req, buffer,
			     KERN_DATA_BUFFER_SIZE, &data_size);

	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not get struct with req %d\r\n",
			__func__,
			struct_req);
		return (-1);
	}

	msg.len = data_size;
	msg.req_type = struct_req;

	rc = migration_send_data_remote(socket, &msg, sizeof(msg));
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not send struct msg for req %d\r\n",
			__func__,
			struct_req);
		return (-1);
	}

	rc = migration_send_data_remote(socket, buffer, data_size);
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not send struct with req %d\r\n",
			__func__,
			struct_req);
		return (-1);
	}

	return (0);
}

static inline int
migrate_recv_kern_struct(struct vmctx *ctx, int socket, char *buffer)
{
	int rc;
	struct migration_message_type msg;

	memset(&msg, 0, sizeof(struct migration_message_type));
	rc = migration_recv_data_from_remote(socket, &msg, sizeof(msg));
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not recv struct mesg\r\n",
			__func__);
		return (-1);
	}
	memset(buffer, 0, KERN_DATA_BUFFER_SIZE);
	rc = migration_recv_data_from_remote(socket, buffer, msg.len);
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not recv struct for req %d\r\n",
			__func__,
			msg.req_type);
		return (-1);
	}

	// restore struct
	rc = vm_restore_req(ctx, msg.req_type, buffer, msg.len);
	if (rc != 0 ) {
		fprintf(stderr,
			"%s: Failed to restore struct %d\r\n",
			__func__,
			msg.req_type);
		return (-1);
	}

	return (0);
}

static int
migrate_kern_data(struct vmctx *ctx, int socket, enum migration_transfer_req req)
{
	int i, rc, error = 0;
	char *buffer;

	buffer = malloc(KERN_DATA_BUFFER_SIZE * sizeof(char));
	if (buffer == NULL) {
		fprintf(stderr,
			"%s: Could not allocate memory\r\n",
			__func__);
		return (-1);
	}

	for (i = 0; i < nitems(snapshot_kern_structs); i++) {
		if (req == MIGRATION_RECV_REQ) {
			// wait for msg message
			rc = migrate_recv_kern_struct(ctx, socket, buffer);
			if (rc < 0) {
				fprintf(stderr,
					"%s: Could not restore struct %s\r\n",
					__func__,
					snapshot_kern_structs[i].struct_name);
				error = -1;
				break;
			}
		} else if (req == MIGRATION_SEND_REQ) {
			rc = migrate_send_kern_struct(ctx, socket, buffer,
					snapshot_kern_structs[i].req);
			if (rc < 0 ) {
				fprintf(stderr,
					"%s: Could not send %s\r\n",
					__func__,
					snapshot_kern_structs[i].struct_name);
				error = -1;
				break;
			}
		} else {
			fprintf(stderr,
				"%s: Unknown transfer request\r\n",
				__func__);
			error = -1;
			break;
		}
	}

	free(buffer);

	return (error);
}

static inline const struct vm_snapshot_dev_info *
find_entry_for_dev(const char *name)
{
	int i;
	for (i = 0; i < nitems(snapshot_devs); i++) {
		if (strncmp(name, snapshot_devs[i].dev_name, MAX_DEV_NAME_LEN) == 0) {
			return (&snapshot_devs[i]);
		}
	}

	return NULL;
}

static inline int
migrate_send_dev(struct vmctx *ctx, int socket, const char *dev,
		     char *buffer, size_t len)
{
	int rc;
	size_t data_size = 0;
	struct migration_message_type msg;
	const struct vm_snapshot_dev_info *dev_info;

	memset(buffer, 0, len);
	dev_info = find_entry_for_dev(dev);
	if (dev_info == NULL) {
	    fprintf(stderr, "%s: Could not find the device %s "
		    "or migration not implemented yet for it."
		    "Please check if you have the same OS version installed.\r\n",
		    __func__, dev);
	    return (0);
	}

	rc = (*dev_info->snapshot_cb)(ctx, dev, buffer, len, &data_size);
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not get info about %s dev\r\n",
			__func__,
			dev);
		return (-1);
	}

	// send struct size to destination
	memset(&msg, 0, sizeof(msg));
	msg.type = MESSAGE_TYPE_DEV;
	msg.len = data_size;
	strncpy(msg.name, dev, MAX_DEV_NAME_LEN);

	rc = migration_send_data_remote(socket, &msg, sizeof(msg));
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not send msg for %s dev\r\n",
			__func__,
			dev);
		return (-1);
	}

	// send dev
	if (data_size == 0) {
		fprintf(stderr, "%s: Did not send %s dev. Assuming unused. Continuing...\r\n", __func__, dev);
		return (0);
	}

	rc = migration_send_data_remote(socket, buffer, data_size);
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not send %s dev\r\n",
			__func__,
			dev);
		return (-1);
	}

	return (0);
}

static int
migrate_recv_dev(struct vmctx *ctx, int socket, char *buffer, size_t len)
{
	int rc;
	size_t data_size;
	struct migration_message_type msg;
	const struct vm_snapshot_dev_info *dev_info;

	// recv struct size to destination
	memset(&msg, 0, sizeof(msg));

	rc = migration_recv_data_from_remote(socket, &msg, sizeof(msg));
	if (rc < 0) {
		fprintf(stderr, "%s: Could not recv msg for device.\r\n", __func__);
		return (-1);
	}

	data_size = msg.len;
	// recv dev

	if(data_size == 0) {
		fprintf(stderr, "%s: Did not restore %s dev. Assuming unused. Continuing...\r\n", __func__, msg.name);
		return (0);
	}

	memset(buffer, 0 , len);
	rc = migration_recv_data_from_remote(socket, buffer, data_size);
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not recv %s dev\r\n",
			__func__,
			msg.name);
		return (-1);
	}

	dev_info = find_entry_for_dev(msg.name);
	if (dev_info == NULL) {
	    fprintf(stderr, "%s: Could not find the device %s "
		    "or migration not implemented yet for it."
		    "Please check if you have the same OS version installed.\r\n",
		    __func__, msg.name);
	    return (0);
	}

	rc = (*dev_info->restore_cb)(ctx, msg.name, buffer, data_size);
	if (rc != 0) {
		fprintf(stderr,
			"%s: Could not restore %s dev\r\n",
			__func__,
			msg.name);
		return (-1);
	}

	return (0);
}


static int
migrate_devs(struct vmctx *ctx, int socket, enum migration_transfer_req req)
{
	int i, num_items;
	int rc, error = 0;
	char *buffer;


	buffer = malloc(SNAPSHOT_BUFFER_SIZE * sizeof(char));
	if (buffer == NULL) {
		fprintf(stderr,
			"%s: Could not allocate memory\r\n",
			__func__);
		error = -1;
		goto end;
	}

	if (req == MIGRATION_SEND_REQ) {
		// send to the destination the number of devices that will
		// be migrated
		num_items = nitems(snapshot_devs);
		rc = migration_send_data_remote(socket, &num_items, sizeof(num_items));

		if (rc < 0) {
		    fprintf(stderr, "%s: Could not send num_items to destination\r\n", __func__);
		    return (-1);
		}

		for (i = 0; i < num_items; i++) {
			rc = migrate_send_dev(ctx, socket, snapshot_devs[i].dev_name,
						buffer, SNAPSHOT_BUFFER_SIZE);

			if (rc < 0) {
				fprintf(stderr,
					"%s: Could not send %s\r\n",
					__func__, snapshot_devs[i].dev_name);
				error = -1;
				goto end;
			}
	    }
	} else if (req == MIGRATION_RECV_REQ) {
		// receive the number of devices that will be migrated
		rc = migration_recv_data_from_remote(socket, &num_items, sizeof(num_items));

		if (rc < 0) {
		    fprintf(stderr, "%s: Could not recv num_items from source\r\n", __func__);
		    return (-1);
		}

		for (i = 0; i < num_items; i ++) {
			rc = migrate_recv_dev(ctx, socket, buffer, SNAPSHOT_BUFFER_SIZE);
			if (rc < 0) {
				fprintf(stderr,
				    "%s: Could not recv device\r\n",
				    __func__);
				error = -1;
				goto end;
			}
		}
	}

	error = 0;

end:
	if (buffer != NULL)
		free(buffer);

	return (error);
}

int
vm_send_migrate_req(struct vmctx *ctx, struct migrate_req req)
{
	unsigned char ipv4_addr[MAX_IP_LEN];
	unsigned char ipv6_addr[MAX_IP_LEN];
	int addr_type;
	struct sockaddr_in sa;
	int s;
	int rc;
	size_t migration_completed;

	rc = get_migration_host_and_type(req.host, ipv4_addr,
					 ipv6_addr, &addr_type);

	if (rc != 0) {
		fprintf(stderr, "%s: Invalid address or not IPv6.", __func__);
		fprintf(stderr, "%s: :IP address used for migration: %s;\r\n"
				"Port used for migration: %d\r\n"
				"Exiting...\r\n",
				__func__,
				req.host,
				req.port);
		return (rc);
	}

	if (addr_type == AF_INET6) {
		fprintf(stderr, "%s: IPv6 is not supported yet for migration. "
				"Please try again using a IPv4 address.\r\n",
				__func__);

		fprintf(stderr, "%s: IP address used for migration: %s;\r\n"
				"Port used for migration: %d\r\n",
				__func__,
				ipv6_addr,
				req.port);
		return (-1);
	}

	fprintf(stdout, "%s: Starting connection to %s on %d port...\r\n",
			__func__, ipv4_addr, req.port);

	/*
	 * Connect to destination host
	 * This host is the client and the remote host is the server
	 */

	s = socket(AF_INET, SOCK_STREAM, 0);

	if (s < 0) {
		perror("Could not create the socket");
		return (-1);
	}

	bzero(&sa, sizeof(sa));

	sa.sin_family = AF_INET;
	sa.sin_port = htons(req.port);

	rc = inet_pton(AF_INET, ipv4_addr, &sa.sin_addr);
	if (rc <= 0) {
		fprintf(stderr, "%s: Could not retrive the IPV4 address", __func__);
		return (-1);
	}

	rc = connect(s, (struct sockaddr *)&sa, sizeof(sa));

	if (rc < 0) {
		perror("Could not connect to the remote host");
		close(s);
		return -1;
	}

	// send system requirements
	rc = migration_send_specs(s);

	if (rc < 0) {
		fprintf(stderr, "%s: Error while checking system requirements\r\n",
			__func__);
		close(s);
		return (rc);
	}

	rc = vm_vcpu_lock_all(ctx);
	if (rc != 0) {
		fprintf(stderr, "%s: Could not suspend vm\r\n", __func__);
		close(s);
		return (rc);
	}

	rc = migrate_send_memory(ctx, s);
	if (rc != 0) {
		fprintf(stderr,
			"%s: Could not send memory to destination\r\n",
			__func__);
		vm_vcpu_unlock_all(ctx);
		close(s);
		return (rc);
	}

	// Send kern data
	rc =  migrate_kern_data(ctx, s, MIGRATION_SEND_REQ);
	if (rc != 0) {
		fprintf(stderr,
			"%s: Could not send kern data to destination\r\n",
			__func__);
		vm_vcpu_unlock_all(ctx);
		close(s);
		return (rc);
	}

	// Send PCI data
	rc =  migrate_devs(ctx, s, MIGRATION_SEND_REQ);
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not send pci devs to destination\r\n",
			__func__);
		vm_vcpu_unlock_all(ctx);
		close(s);
		return (rc);
	}

	// Wait for migration completed	
	rc = migration_recv_data_from_remote(s, &migration_completed,
					sizeof(migration_completed));
	if (rc < 0 || (migration_completed != MIGRATION_SPECS_OK)) {
		fprintf(stderr,
			"%s: Could not recv migration completed remote"
			" or received error\r\n",
			__func__);
		close(s);
		return (-1);
	}

	// Poweroff the vm

	rc = vm_suspend(ctx, VM_SUSPEND_POWEROFF);
	if (rc != 0) {
		fprintf(stderr, "Failed to suspend vm\n");
	}

	vm_vcpu_unlock_all(ctx);

	/* Wait for CPUs to suspend. TODO: write this properly. */
	sleep(5);
	vm_destroy(ctx);
	exit(0);
	// TODO: implement properly return with labels
	// TODO: free properly all the resources
	close(s);
	return (0);
}

int
vm_recv_migrate_req(struct vmctx *ctx, struct migrate_req req)
{
	unsigned char ipv4_addr[MAX_IP_LEN];
	unsigned char ipv6_addr[MAX_IP_LEN];
	int addr_type;
	int s, con_socket;
	struct sockaddr_in sa, client_sa;
	socklen_t client_len;
	int rc;
	size_t migration_completed;

	rc = get_migration_host_and_type(req.host, ipv4_addr,
					 ipv6_addr, &addr_type);

	if (rc != 0) {
		fprintf(stderr, "%s: Invalid address or not IPv6.\r\n", __func__);
		fprintf(stderr, "%s: IP address used for migration: %s;\r\n"
				"Port used for migration: %d\r\n"
				"Exiting...\r\n",
				__func__,
				req.host,
				req.port);
		return (rc);
	}

	if (addr_type == AF_INET6) {
		fprintf(stderr, "%s: IPv6 is not supported yet for migration. "
				"Please try again using a IPv4 address.\r\n",
				__func__);

		fprintf(stderr, "%s: IP address used for migration: %s;\r\n"
				"Port used for migration: %d\r\n",
				__func__,
				ipv6_addr,
				req.port);
		return (-1);
	}

	fprintf(stdout, "%s: Waiting for connections from %s on %d port...\r\n",
			__func__, ipv4_addr, req.port);

	s = socket(AF_INET, SOCK_STREAM, 0);

	if (s < 0) {
		perror("Could not create socket");
		return (-1);
	}

	bzero(&sa, sizeof(sa));

	sa.sin_family = AF_INET;
	sa.sin_port = htons(req.port);
	sa.sin_addr.s_addr = htonl(INADDR_ANY);

	rc = bind(s , (struct sockaddr *)&sa, sizeof(sa));

	if (rc < 0) {
		perror("Could not bind");
		close(s);
		return (-1);
	}

	listen(s, 1);

	con_socket = accept(s, (struct sockaddr *)&client_sa, &client_len);

	if (con_socket < 0) {
		fprintf(stderr, "%s: Could not accept connection\r\n", __func__);
		close(s);
		return (-1);
	}

	rc = migration_recv_and_check_specs(con_socket);
	if (rc < 0) {
		fprintf(stderr, "%s: Error while checking specs\r\n", __func__);
		close(con_socket);
		close(s);
		return (rc);
	}

	rc = migrate_recv_memory(ctx, con_socket);
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not recv lowmem and highmem\r\n",
			__func__);
		close(con_socket);
		close(s);
		return (-1);
	}

	rc = migrate_kern_data(ctx, con_socket, MIGRATION_RECV_REQ);
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not recv kern data\r\n",
			__func__);
		close(con_socket);
		close(s);
		return (-1);
	}

	rc = migrate_devs(ctx, con_socket, MIGRATION_RECV_REQ);
	if (rc < 0) {
		fprintf(stderr,
			"%s: Could not recv pci devs\r\n",
			__func__);
		close(con_socket);
		close(s);
		return (-1);
	}

	fprintf(stdout, "%s: Migration completed\r\n", __func__);
	migration_completed = MIGRATION_SPECS_OK;
	rc = migration_send_data_remote(con_socket, &migration_completed,
					sizeof(migration_completed));
	if (rc < 0 ) {
		fprintf(stderr,
			"%s: Could not send migration completed remote\r\n",
			__func__);
		close(con_socket);
		close(s);
		return (-1);
	}

	// wait for source vm to be destroyed
	sleep(5);
	close(con_socket);
	close(s);
	return (0);
}

int
snapshot_part(volatile void *data, size_t data_size,
	      uint8_t **buffer, size_t *buf_size,
	      size_t *snapshot_len)
{
	size_t idx;

	if (*buf_size < data_size) {
		fprintf(stderr, "%s: buffer too small\r\n", __func__);
		return (-1);
	}

	for (idx = 0; idx < data_size; idx++)
		(*buffer)[idx] = ((uint8_t *) data)[idx];

	*buffer += data_size;
	*buf_size -= data_size;
	*snapshot_len += data_size;

	return (0);
}

int
restore_part(volatile void *data, size_t data_size, uint8_t **buffer,
	     size_t *buf_size)
{
	size_t idx;

	if (*buf_size < data_size) {
		fprintf(stderr, "%s: buffer too small\r\n", __func__);
		return (-1);
	}

	for (idx = 0; idx < data_size; idx++)
		((uint8_t *) data)[idx] = (*buffer)[idx];

	*buffer += data_size;
	*buf_size -= data_size;

	return (0);
}
