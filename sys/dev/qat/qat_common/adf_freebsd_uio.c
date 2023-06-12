/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include "qat_freebsd.h"
#include "adf_cfg.h"
#include "adf_common_drv.h"
#include "adf_accel_devices.h"
#include "icp_qat_uclo.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_init_admin.h"
#include "adf_cfg_strings.h"
#include "adf_uio_control.h"
#include "adf_uio_cleanup.h"
#include "adf_uio.h"
#include "adf_transport_access_macros.h"
#include "adf_transport_internal.h"
#include <sys/conf.h>
#include <sys/capsicum.h>
#include <sys/kdb.h>
#include <sys/condvar.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/file.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/sglist.h>
#include <vm/vm.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

#define ADF_UIO_GET_NAME(accel_dev) (GET_HW_DATA(accel_dev)->dev_class->name)
#define ADF_UIO_GET_TYPE(accel_dev) (GET_HW_DATA(accel_dev)->dev_class->type)
#define ADF_UIO_GET_BAR(accel_dev)                                             \
	(GET_HW_DATA(accel_dev)->get_etr_bar_id(GET_HW_DATA(accel_dev)))

static d_ioctl_t adf_uio_ioctl;
static d_mmap_single_t adf_uio_mmap_single;

static struct cdevsw adf_uio_cdevsw = { .d_ioctl = adf_uio_ioctl,
					.d_mmap_single = adf_uio_mmap_single,
					.d_version = D_VERSION,
					.d_name = "qat" };

struct adf_uio_open_bundle {
	struct adf_uio_control_accel *accel;
	int bundle;
	struct file **mem_files;
	int num_mem_files;
};

static void
adf_release_bundle(void *arg)
{
	struct adf_uio_control_accel *accel = NULL;
	struct adf_uio_open_bundle *handle = NULL;
	struct adf_uio_control_bundle *bundle = NULL;
	struct adf_uio_instance_rings *instance_rings, *tmp;
	int i = 0;

	handle = arg;
	accel = handle->accel;
	bundle = &accel->bundle[handle->bundle];

	mutex_lock(&bundle->lock);
	adf_uio_do_cleanup_orphan(bundle->hardware_bundle_number, accel);
	mutex_unlock(&bundle->lock);

	for (i = 0; i < handle->num_mem_files; i++) {
		/*
		 * Similar to the garbage collection of orphaned file
		 * descriptor references in UNIX domain socket control
		 * messages, the current thread isn't relevant to the
		 * the file descriptor reference being released.  In
		 * particular, the current thread does not hold any
		 * advisory file locks on these file descriptors.
		 */
		fdrop(handle->mem_files[i], NULL);
	}
	free(handle->mem_files, M_QAT);

	mtx_lock(&accel->lock);

	mutex_lock(&bundle->list_lock);
	list_for_each_entry_safe(instance_rings, tmp, &bundle->list, list)
	{
		if (instance_rings->user_pid == curproc->p_pid) {
			list_del(&instance_rings->list);
			free(instance_rings, M_QAT);
			break;
		}
	}
	mutex_unlock(&bundle->list_lock);

	adf_dev_put(accel->accel_dev);
	accel->num_handles--;
	free(handle, M_QAT);
	if (!accel->num_handles) {
		cv_broadcast(&accel->cleanup_ok);
		/* the broadcasting effect happens after releasing accel->lock
		 */
	}
	mtx_unlock(&accel->lock);
}

static int
adf_add_mem_fd(struct adf_accel_dev *accel_dev, int mem_fd)
{
	struct adf_uio_control_accel *accel = NULL;
	struct adf_uio_open_bundle *handle = NULL;
	struct file *fp, **new_files;
	cap_rights_t rights;
	int error = -1, old_count = 0;

	error = devfs_get_cdevpriv((void **)&handle);
	if (error)
		return (error);

	error = fget(curthread, mem_fd, cap_rights_init(&rights), &fp);
	if (error) {
		printf(
		    "Failed to fetch file pointer from current process %d \n",
		    __LINE__);
		return (error);
	}

	accel = accel_dev->accel;
	mtx_lock(&accel->lock);
	for (;;) {
		old_count = handle->num_mem_files;
		mtx_unlock(&accel->lock);
		new_files = malloc((old_count + 1) * sizeof(*new_files),
				   M_QAT,
				   M_WAITOK);
		mtx_lock(&accel->lock);
		if (old_count == handle->num_mem_files) {
			if (old_count != 0) {
				memcpy(new_files,
				       handle->mem_files,
				       old_count * sizeof(*new_files));
				free(handle->mem_files, M_QAT);
			}
			handle->mem_files = new_files;
			new_files[old_count] = fp;
			handle->num_mem_files++;
			break;
		} else
			free(new_files, M_QAT);
	}
	mtx_unlock(&accel->lock);
	return (0);
}

static vm_object_t
adf_uio_map_bar(struct adf_accel_dev *accel_dev, uint8_t bank_offset)
{
	unsigned int ring_bundle_size, offset;
	struct sglist *sg = NULL;
	struct adf_uio_control_accel *accel = accel_dev->accel;
	struct adf_hw_csr_info *csr_info = &accel_dev->hw_device->csr_info;
	vm_object_t obj;

	ring_bundle_size = csr_info->ring_bundle_size;
	offset = bank_offset * ring_bundle_size;

	sg = sglist_alloc(1, M_WAITOK);

	/* Starting from new HW there is an additional offset
	 * for bundle CSRs
	 */
	sglist_append_phys(sg,
			   accel->bar->base_addr + offset +
			       csr_info->csr_addr_offset,
			   ring_bundle_size);

	obj = vm_pager_allocate(
	    OBJT_SG, sg, ring_bundle_size, VM_PROT_RW, 0, NULL);
	if (obj != NULL) {
		VM_OBJECT_WLOCK(obj);
		vm_object_set_memattr(obj, VM_MEMATTR_UNCACHEABLE);
		VM_OBJECT_WUNLOCK(obj);
	}
	sglist_free(sg);

	return obj;
}

static int
adf_alloc_bundle(struct adf_accel_dev *accel_dev, int bundle_nr)
{
	struct adf_uio_control_accel *accel = NULL;
	struct adf_uio_open_bundle *handle = NULL;
	int error;

	if (bundle_nr < 0 || bundle_nr >= GET_MAX_BANKS(accel_dev)) {
		printf("ERROR in %s (%d) %d\n", __func__, bundle_nr, __LINE__);
		return EINVAL;
	}

	accel = accel_dev->accel;
	handle = malloc(sizeof(*handle), M_QAT, M_WAITOK | M_ZERO);
	if (!handle) {
		printf("ERROR in adf_alloc_bundle %d\n", __LINE__);
		return ENOMEM;
	}
	handle->accel = accel;
	handle->bundle = bundle_nr;

	mtx_lock(&accel->lock);
	adf_dev_get(accel_dev);
	accel->num_handles++;
	mtx_unlock(&accel->lock);

	error = devfs_set_cdevpriv(handle, adf_release_bundle);
	if (error) {
		adf_release_bundle(handle);
		device_printf(GET_DEV(accel_dev),
			      "ERROR in adf_alloc_bundle %d\n",
			      __LINE__);
		return (error);
	}

	return (0);
}

static int
adf_uio_ioctl(struct cdev *dev,
	      u_long cmd,
	      caddr_t data,
	      int fflag,
	      struct thread *td)
{
	struct adf_accel_dev *accel_dev = dev->si_drv1;
	struct adf_hw_csr_info *csr_info = NULL;

	if (!accel_dev) {
		printf("%s - accel_dev is NULL\n", __func__);
		return EFAULT;
	}

	csr_info = &accel_dev->hw_device->csr_info;

	switch (cmd) {
	case IOCTL_GET_BUNDLE_SIZE:
		*(uint32_t *)data = csr_info->ring_bundle_size;
		break;
	case IOCTL_ALLOC_BUNDLE:
		return (adf_alloc_bundle(accel_dev, *(int *)data));
	case IOCTL_GET_ACCEL_TYPE:
		*(uint32_t *)data = ADF_UIO_GET_TYPE(accel_dev);
		break;
	case IOCTL_ADD_MEM_FD:
		return (adf_add_mem_fd(accel_dev, *(int *)data));
	default:
		return (ENOTTY);
	}
	return (0);
}

static int
adf_uio_mmap_single(struct cdev *dev,
		    vm_ooffset_t *offset,
		    vm_size_t size,
		    struct vm_object **object,
		    int nprot)
{
	struct adf_uio_open_bundle *handle = NULL;
	struct adf_uio_control_accel *accel = NULL;
	struct adf_uio_control_bundle *bundle = NULL;
	struct adf_uio_instance_rings *instance_rings;
	int error;

	error = devfs_get_cdevpriv((void **)&handle);
	if (error)
		return (error);

	if (!handle->accel) {
		printf("QAT: Error - no accel in handle\n");
		return EINVAL;
	}
	accel = handle->accel;

	if (!accel->accel_dev) {
		printf("QAT: Error - no accel_dev in accel\n");
		return EINVAL;
	}

	bundle = &accel->bundle[handle->bundle];
	if (!bundle->obj) {
		printf("QAT: Error no vm_object in bundle\n");
		return EINVAL;
	}

	/* Adding pid to bundle list */
	instance_rings =
	    malloc(sizeof(*instance_rings), M_QAT, M_WAITOK | M_ZERO);
	if (!instance_rings) {
		printf("QAT: Memory allocation error - line: %d\n", __LINE__);
		return -ENOMEM;
	}
	instance_rings->user_pid = curproc->p_pid;
	instance_rings->ring_mask = 0;
	mutex_lock(&bundle->list_lock);
	list_add_tail(&instance_rings->list, &bundle->list);
	mutex_unlock(&bundle->list_lock);

	vm_object_reference(bundle->obj);
	*object = bundle->obj;
	return (0);
}

static inline void
adf_uio_init_accel_ctrl(struct adf_uio_control_accel *accel,
			struct adf_accel_dev *accel_dev,
			unsigned int nb_bundles)
{
	struct adf_uio_control_bundle *bundle;
	struct qat_uio_bundle_dev *priv;
	unsigned int i;

	accel->nb_bundles = nb_bundles;
	accel->total_used_bundles = 0;

	for (i = 0; i < nb_bundles; i++) {
		/*initialize the bundle */
		bundle = &accel->bundle[i];
		priv = &bundle->uio_priv;
		bundle->hardware_bundle_number =
		    GET_MAX_BANKS(accel_dev) - nb_bundles + i;

		INIT_LIST_HEAD(&bundle->list);
		priv->bundle = bundle;
		priv->accel = accel;

		mutex_init(&bundle->lock);
		mutex_init(&bundle->list_lock);
		if (!accel->bar)
			printf("ERROR: bar not defined in accel\n");
		else
			bundle->csr_addr = (void *)accel->bar->virt_addr;
	}
}

/**
 * Initialization bars on dev start.
 */
static inline void
adf_uio_init_bundle_dev(struct adf_uio_control_accel *accel,
			struct adf_accel_dev *accel_dev,
			unsigned int nb_bundles)
{
	struct adf_uio_control_bundle *bundle;
	unsigned int i;

	for (i = 0; i < nb_bundles; i++) {
		bundle = &accel->bundle[i];
		bundle->obj =
		    adf_uio_map_bar(accel_dev, bundle->hardware_bundle_number);
		if (!bundle->obj) {
			device_printf(GET_DEV(accel_dev),
				      "ERROR in adf_alloc_bundle %d\n",
				      __LINE__);
		}
	}
}

int
adf_uio_register(struct adf_accel_dev *accel_dev)
{
	struct adf_uio_control_accel *accel = NULL;
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	int nb_bundles;

	if (!accel_dev) {
		printf("%s - accel_dev is NULL\n", __func__);
		return EFAULT;
	}

	if (adf_cfg_get_param_value(
		accel_dev, ADF_GENERAL_SEC, ADF_FIRST_USER_BUNDLE, val)) {
		nb_bundles = 0;
	} else {
		nb_bundles = GET_MAX_BANKS(accel_dev);
	}

	if (nb_bundles) {
		accel = malloc(sizeof(*accel) +
				   nb_bundles *
				       sizeof(struct adf_uio_control_bundle),
			       M_QAT,
			       M_WAITOK | M_ZERO);
		mtx_init(&accel->lock, "qat uio", NULL, MTX_DEF);
		accel->accel_dev = accel_dev;
		accel->bar = accel_dev->accel_pci_dev.pci_bars +
		    ADF_UIO_GET_BAR(accel_dev);

		adf_uio_init_accel_ctrl(accel, accel_dev, nb_bundles);
		accel->cdev = make_dev(&adf_uio_cdevsw,
				       0,
				       UID_ROOT,
				       GID_WHEEL,
				       0600,
				       "%s",
				       device_get_nameunit(GET_DEV(accel_dev)));
		if (accel->cdev == NULL) {
			mtx_destroy(&accel->lock);
			goto fail_clean;
		}
		accel->cdev->si_drv1 = accel_dev;
		accel_dev->accel = accel;
		cv_init(&accel->cleanup_ok, "uio_accel_cv");

		adf_uio_init_bundle_dev(accel, accel_dev, nb_bundles);
	}
	return 0;
fail_clean:
	free(accel, M_QAT);
	device_printf(GET_DEV(accel_dev), "Failed to register UIO devices\n");
	return ENODEV;
}

void
adf_uio_remove(struct adf_accel_dev *accel_dev)
{
	struct adf_uio_control_accel *accel = accel_dev->accel;
	struct adf_uio_control_bundle *bundle;
	unsigned int i;

	if (accel) {
		/* Un-mapping all bars */
		for (i = 0; i < accel->nb_bundles; i++) {
			bundle = &accel->bundle[i];
			vm_object_deallocate(bundle->obj);
		}

		destroy_dev(accel->cdev);
		mtx_lock(&accel->lock);
		while (accel->num_handles) {
			cv_timedwait_sig(&accel->cleanup_ok,
					 &accel->lock,
					 3 * hz);
		}
		mtx_unlock(&accel->lock);
		mtx_destroy(&accel->lock);
		cv_destroy(&accel->cleanup_ok);
		free(accel, M_QAT);
		accel_dev->accel = NULL;
	}
}
