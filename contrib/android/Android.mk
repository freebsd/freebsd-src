#
# Copyright (C) 2014 Trevor Drake
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#


# A bit of a non-standard LOCAL_PATH declaration here
# The Android.mk lives below the top source directory
# but LOCAL_PATH needs to point to the top of the module
# source tree to maintain the integrity of the intermediates
# directories
LOCAL_PATH := $(subst /contrib/android,,$(call my-dir))

libarchive_target_config := contrib/android/config/android.h

libarchive_src_files := libarchive/archive_acl.c \
						libarchive/archive_check_magic.c \
						libarchive/archive_cmdline.c \
						libarchive/archive_cryptor.c \
						libarchive/archive_digest.c \
						libarchive/archive_entry.c \
						libarchive/archive_entry_copy_stat.c \
						libarchive/archive_entry_link_resolver.c \
						libarchive/archive_entry_sparse.c \
						libarchive/archive_entry_stat.c \
						libarchive/archive_entry_strmode.c \
						libarchive/archive_entry_xattr.c \
						libarchive/archive_getdate.c \
						libarchive/archive_hmac.c \
						libarchive/archive_match.c \
						libarchive/archive_options.c \
						libarchive/archive_pack_dev.c \
						libarchive/archive_pathmatch.c \
						libarchive/archive_ppmd7.c \
						libarchive/archive_random.c \
						libarchive/archive_rb.c \
						libarchive/archive_read.c \
						libarchive/archive_read_add_passphrase.c \
						libarchive/archive_read_append_filter.c \
						libarchive/archive_read_data_into_fd.c \
						libarchive/archive_read_disk_entry_from_file.c \
						libarchive/archive_read_disk_posix.c \
						libarchive/archive_read_disk_set_standard_lookup.c \
						libarchive/archive_read_extract.c \
						libarchive/archive_read_extract2.c \
						libarchive/archive_read_open_fd.c \
						libarchive/archive_read_open_file.c \
						libarchive/archive_read_open_filename.c \
						libarchive/archive_read_open_memory.c \
						libarchive/archive_read_set_format.c \
						libarchive/archive_read_set_options.c \
						libarchive/archive_read_support_filter_all.c \
						libarchive/archive_read_support_filter_bzip2.c \
						libarchive/archive_read_support_filter_compress.c \
						libarchive/archive_read_support_filter_grzip.c \
						libarchive/archive_read_support_filter_gzip.c \
						libarchive/archive_read_support_filter_lrzip.c \
						libarchive/archive_read_support_filter_lz4.c \
						libarchive/archive_read_support_filter_lzop.c \
						libarchive/archive_read_support_filter_none.c \
						libarchive/archive_read_support_filter_program.c \
						libarchive/archive_read_support_filter_rpm.c \
						libarchive/archive_read_support_filter_uu.c \
						libarchive/archive_read_support_filter_xz.c \
						libarchive/archive_read_support_filter_zstd.c \
						libarchive/archive_read_support_format_7zip.c \
						libarchive/archive_read_support_format_all.c \
						libarchive/archive_read_support_format_ar.c \
						libarchive/archive_read_support_format_by_code.c \
						libarchive/archive_read_support_format_cab.c \
						libarchive/archive_read_support_format_cpio.c \
						libarchive/archive_read_support_format_empty.c \
						libarchive/archive_read_support_format_iso9660.c \
						libarchive/archive_read_support_format_lha.c \
						libarchive/archive_read_support_format_mtree.c \
						libarchive/archive_read_support_format_rar.c \
						libarchive/archive_read_support_format_raw.c \
						libarchive/archive_read_support_format_tar.c \
						libarchive/archive_read_support_format_warc.c \
						libarchive/archive_read_support_format_xar.c \
						libarchive/archive_read_support_format_zip.c \
						libarchive/archive_string.c \
						libarchive/archive_string_sprintf.c \
						libarchive/archive_util.c \
						libarchive/archive_version_details.c \
						libarchive/archive_virtual.c \
						libarchive/archive_write.c \
						libarchive/archive_write_disk_posix.c \
						libarchive/archive_write_disk_set_standard_lookup.c \
						libarchive/archive_write_open_fd.c \
						libarchive/archive_write_open_file.c \
						libarchive/archive_write_open_filename.c \
						libarchive/archive_write_open_memory.c \
						libarchive/archive_write_add_filter.c \
						libarchive/archive_write_add_filter_b64encode.c \
						libarchive/archive_write_add_filter_by_name.c \
						libarchive/archive_write_add_filter_bzip2.c \
						libarchive/archive_write_add_filter_compress.c \
						libarchive/archive_write_add_filter_grzip.c \
						libarchive/archive_write_add_filter_gzip.c \
						libarchive/archive_write_add_filter_lrzip.c \
						libarchive/archive_write_add_filter_lz4.c \
						libarchive/archive_write_add_filter_lzop.c \
						libarchive/archive_write_add_filter_none.c \
						libarchive/archive_write_add_filter_program.c \
						libarchive/archive_write_add_filter_uuencode.c \
						libarchive/archive_write_add_filter_xz.c \
						libarchive/archive_write_add_filter_zstd.c \
						libarchive/archive_write_set_format.c \
						libarchive/archive_write_set_format_7zip.c \
						libarchive/archive_write_set_format_ar.c \
						libarchive/archive_write_set_format_by_name.c \
						libarchive/archive_write_set_format_cpio.c \
						libarchive/archive_write_set_format_cpio_newc.c \
						libarchive/archive_write_set_format_iso9660.c \
						libarchive/archive_write_set_format_mtree.c \
						libarchive/archive_write_set_format_pax.c \
						libarchive/archive_write_set_format_raw.c \
						libarchive/archive_write_set_format_shar.c \
						libarchive/archive_write_set_format_ustar.c \
						libarchive/archive_write_set_format_v7tar.c \
						libarchive/archive_write_set_format_gnutar.c \
						libarchive/archive_write_set_format_warc.c \
						libarchive/archive_write_set_format_xar.c \
						libarchive/archive_write_set_format_zip.c \
						libarchive/archive_write_set_options.c \
						libarchive/archive_write_set_passphrase.c \
						libarchive/filter_fork_posix.c \
						libarchive/xxhash.c

ifeq ($(HOST_OS),windows)
libarchive_host_src_files := \
							libarchive/archive_entry_copy_bhfi.c \
							libarchive/archive_read_disk_windows.c \
							libarchive/archive_write_disk_windows.c \
							libarchive/filter_fork_windows.c \
							libarchive/archive_windows.c
else
libarchive_host_src_files :=
endif

libarchive_fe_src_files :=  libarchive_fe/err.c \
							libarchive_fe/line_reader.c \
							libarchive_fe/passphrase.c

bsdtar_src_files := tar/bsdtar.c \
					tar/bsdtar_windows.c \
					tar/cmdline.c \
					tar/creation_set.c \
					tar/read.c \
					tar/subst.c \
					tar/util.c \
					tar/write.c

bsdcpio_src_files := cpio/cmdline.c \
					cpio/cpio.c

bsdcat_src_files := cat/cmdline.c \
					cat/bsdcat.c


ifeq ($(HOST_OS),darwin)
$(warning Host : $(HOST_OS) Not Supported. Host Build Will Be Skipped )
else
libarchive_host_config := contrib/android/config/$(HOST_OS)_host.h

include $(CLEAR_VARS)
LOCAL_MODULE := libarchive
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := $(libarchive_src_files) $(libarchive_host_src_files)
LOCAL_CFLAGS := -DPLATFORM_CONFIG_H=\"$(libarchive_host_config)\"
LOCAL_C_INCLUDES := $(LOCAL_PATH)/contrib/android/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/libarchive
include $(BUILD_HOST_STATIC_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libarchive
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -DPLATFORM_CONFIG_H=\"$(libarchive_host_config)\"
LOCAL_SHARED_LIBRARIES := libz-host
LOCAL_WHOLE_STATIC_LIBRARIES := libarchive
LOCAL_C_INCLUDES := $(LOCAL_PATH)/contrib/android/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/libarchive
include $(BUILD_HOST_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libarchive_fe
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -DPLATFORM_CONFIG_H=\"$(libarchive_host_config)\"
LOCAL_SRC_FILES := $(libarchive_fe_src_files)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/contrib/android/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/libarchive_fe
include $(BUILD_HOST_STATIC_LIBRARY)

endif


# Do not build target binaries if we are not targeting linux
# on the host
ifeq ($(HOST_OS),linux)

include $(CLEAR_VARS)
LOCAL_MODULE := bsdtar
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS :=  -DBSDTAR_VERSION_STRING=ARCHIVE_VERSION_ONLY_STRING -DPLATFORM_CONFIG_H=\"$(libarchive_host_config)\"
LOCAL_SHARED_LIBRARIES := libz-host
LOCAL_STATIC_LIBRARIES := libarchive libarchive_fe
LOCAL_SRC_FILES := $(bsdtar_src_files)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/contrib/android/include
include $(BUILD_HOST_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := bsdcpio
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS :=  -DBSDCPIO_VERSION_STRING=ARCHIVE_VERSION_ONLY_STRING -DPLATFORM_CONFIG_H=\"$(libarchive_host_config)\"
LOCAL_SHARED_LIBRARIES := libz-host
LOCAL_STATIC_LIBRARIES := libarchive libarchive_fe
LOCAL_SRC_FILES := $(bsdcpio_src_files)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/contrib/android/include
include $(BUILD_HOST_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := bsdcat
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -DBSDCAT_VERSION_STRING=ARCHIVE_VERSION_ONLY_STRING -DPLATFORM_CONFIG_H=\"$(libarchive_host_config)\"
LOCAL_SHARED_LIBRARIES := libz-host
LOCAL_STATIC_LIBRARIES := libarchive libarchive_fe
LOCAL_SRC_FILES := $(bsdcat_src_files)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/contrib/android/include
include $(BUILD_HOST_EXECUTABLE)



include $(CLEAR_VARS)
LOCAL_MODULE := libarchive
LOCAL_MODULE_TAGS := optional
LOCAL_SRC_FILES := $(libarchive_src_files)
LOCAL_STATIC_LIBRARIES := libz liblz4
LOCAL_CFLAGS := -DPLATFORM_CONFIG_H=\"$(libarchive_target_config)\"
LOCAL_C_INCLUDES := $(LOCAL_PATH)/contrib/android/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/libarchive
include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := libarchive
LOCAL_MODULE_TAGS := optional
LOCAL_C_INCLUDES :=
LOCAL_CFLAGS := -DPLATFORM_CONFIG_H=\"$(libarchive_target_config)\"
LOCAL_SHARED_LIBRARIES := libz
LOCAL_WHOLE_STATIC_LIBRARIES := libarchive
LOCAL_C_INCLUDES := $(LOCAL_PATH)/contrib/android/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/libarchive
include $(BUILD_SHARED_LIBRARY)

include $(CLEAR_VARS)
LOCAL_MODULE := libarchive_fe
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -DPLATFORM_CONFIG_H=\"$(libarchive_target_config)\"
LOCAL_SRC_FILES := $(libarchive_fe_src_files)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/contrib/android/include
LOCAL_EXPORT_C_INCLUDE_DIRS := $(LOCAL_PATH)/libarchive_fe
include $(BUILD_STATIC_LIBRARY)


include $(CLEAR_VARS)
LOCAL_MODULE := bsdtar
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS :=  -DBSDTAR_VERSION_STRING=ARCHIVE_VERSION_ONLY_STRING -DPLATFORM_CONFIG_H=\"$(libarchive_target_config)\"
LOCAL_SHARED_LIBRARIES := libz
LOCAL_STATIC_LIBRARIES := libarchive libarchive_fe
LOCAL_SRC_FILES := $(bsdtar_src_files)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/libarchive $(LOCAL_PATH)/libarchive_fe $(LOCAL_PATH)/contrib/android/include
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := bsdcpio
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS :=  -DBSDCPIO_VERSION_STRING=ARCHIVE_VERSION_ONLY_STRING -DPLATFORM_CONFIG_H=\"$(libarchive_target_config)\"
LOCAL_SHARED_LIBRARIES := libz
LOCAL_STATIC_LIBRARIES := libarchive libarchive_fe
LOCAL_SRC_FILES := $(bsdcpio_src_files)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/libarchive $(LOCAL_PATH)/libarchive_fe $(LOCAL_PATH)/contrib/android/include
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)
LOCAL_MODULE := bsdcat
LOCAL_MODULE_TAGS := optional
LOCAL_CFLAGS := -DBSDCAT_VERSION_STRING=ARCHIVE_VERSION_ONLY_STRING -DPLATFORM_CONFIG_H=\"$(libarchive_target_config)\"
LOCAL_SHARED_LIBRARIES := libz
LOCAL_STATIC_LIBRARIES := libarchive libarchive_fe
LOCAL_SRC_FILES := $(bsdcat_src_files)
LOCAL_C_INCLUDES := $(LOCAL_PATH)/libarchive $(LOCAL_PATH)/libarchive_fe $(LOCAL_PATH)/contrib/android/include
include $(BUILD_EXECUTABLE)

endif
