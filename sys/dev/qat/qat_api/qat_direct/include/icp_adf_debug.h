/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/******************************************************************************
 * @file icp_adf_debug.h
 *
 * @description
 *      This header file that contains the prototypes and definitions required
 *      for ADF debug feature.
 *
*****************************************************************************/
#ifndef ICP_ADF_DEBUG_H
#define ICP_ADF_DEBUG_H

/*
 * adf_proc_type_t
 * Type of proc file. Simple for files where read funct
 * prints less than page size (4kB) and seq type for files
 * where read function needs to print more that page size.
 */
typedef enum adf_proc_type_e {
	ADF_PROC_SIMPLE = 1,
	ADF_PROC_SEQ
} adf_proc_type_t;

/*
 * debug_dir_info_t
 * Struct which is used to hold information about a debug directory
 * under the proc filesystem.
 * Client should only set name and parent fields.
 */
typedef struct debug_dir_info_s {
	char *name;
	struct debug_dir_info_s *parent;
	/* The below fields are used internally by the driver */
	struct debug_dir_info_s *dirChildListHead;
	struct debug_dir_info_s *dirChildListTail;
	struct debug_dir_info_s *pNext;
	struct debug_dir_info_s *pPrev;
	struct debug_file_info_s *fileListHead;
	struct debug_file_info_s *fileListTail;
	void *proc_entry;
} debug_dir_info_t;

/*
 * Read handle type for simple proc file
 * Function is called only once and can print up to 4kB (size)
 * Function should return number of bytes printed.
 */
typedef int (*file_read)(void *private_data, char *buff, int size);

/*
 * Read handle type for sequential proc file
 * Function can be called more than once. It will be called until the
 * return value is not 0. offset should be used to mark the starting
 * point for next step. In one go function can print up to 4kB (size).
 * Function should return 0 (zero) if all info is printed or
 * offset from where to start in next step.
 */
typedef int (*file_read_seq)(void *private_data,
			     char *buff,
			     int size,
			     int offset);

/*
 * debug_file_info_t
 * Struct which is used to hold information about a debug file
 * under the proc filesystem.
 * Client should only set name, type, private_data, parent fields,
 * and read or seq_read pointers depending on type used.
 */
typedef struct debug_file_info_s {
	char *name;
	struct debug_dir_info_s *parent;
	adf_proc_type_t type;
	file_read read;
	file_read_seq seq_read;
	void *private_data;
	/* The below fields are used internally by the driver */
	struct debug_file_info_s *pNext;
	struct debug_file_info_s *pPrev;
	void *page;
	Cpa32U offset;
	void *proc_entry;
} debug_file_info_t;

/*
 * icp_adf_debugAddDir
 *
 * Description:
 *  Function used by subsystem to register a new
 *  directory under the proc filesystem
 *
 * Returns:
 *   CPA_STATUS_SUCCESS    on success
 *   CPA_STATUS_FAIL       on failure
 */
CpaStatus icp_adf_debugAddDir(icp_accel_dev_t *accel_dev,
			      debug_dir_info_t *dir_info);

/*
 * icp_adf_debugRemoveDir
 *
 * Description:
 *  Function used by subsystem to remove an existing
 *  directory for which debug output may be stored
 *  in the proc filesystem.
 *
*/
void icp_adf_debugRemoveDir(debug_dir_info_t *dir_info);

/*
 * icp_adf_debugAddFile
 *
 * Description:
 *  Function used by subsystem to add a new file under
 *  the proc file system in which debug output may be written
 *
 * Returns:
 *   CPA_STATUS_SUCCESS   on success
 *   CPA_STATUS_FAIL      on failure
 */
CpaStatus icp_adf_debugAddFile(icp_accel_dev_t *accel_dev,
			       debug_file_info_t *file_info);

/*
 * icp_adf_debugRemoveFile
 *
 * Description:
 *  Function used by subsystem to remove an existing file under
 *  the proc filesystem in which debug output may be written
 *
 */
void icp_adf_debugRemoveFile(debug_file_info_t *file_info);

#endif /* ICP_ADF_DEBUG_H */
