/*
 * Copyright (c) 2026 The FreeBSD Mobile Project
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "gpu_detect.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

/* Helper to read sysfs integer */
static int
read_sysfs_int(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return -1;

    char buf[32];
    ssize_t len = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (len <= 0)
        return -1;

    buf[len] = '\0';
    return atoi(buf);
}

/* Helper to read sysfs string */
static char *
read_sysfs_string(const char *path)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0)
        return NULL;

    char buf[256];
    ssize_t len = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (len <= 0)
        return NULL;

    buf[len] = '\0';
    return strdup(buf);
}

/* Probe GPU from DRM device */
static bool
probe_drm_gpu(struct gpu_info *info, const char *card_path)
{
    char path[256];
    int vendor, device;

    /* Read vendor */
    snprintf(path, sizeof(path), "%s/device/vendor", card_path);
    vendor = read_sysfs_int(path);
    if (vendor < 0)
        return false;
    info->vendor = vendor;

    /* Read device */
    snprintf(path, sizeof(path), "%s/device/device", card_path);
    device = read_sysfs_int(path);
    if (device < 0)
        return false;
    info->device = device;

    /* Determine name */
    switch (vendor) {
    case 0x8086:
        info->name = "Intel";
        break;
    case 0x1022:
        info->name = "AMD";
        break;
    case 0x13B5:
        info->name = "ARM";
        break;
    case 0x5143:
        info->name = "Qualcomm";
        break;
    default:
        info->name = "Unknown";
        break;
    }

    /* Check for 3D/GL by looking at driver name */
    snprintf(path, sizeof(path), "%s/device/driver/module", card_path);
    if (access(path, F_OK) == 0) {
        char driver_path[256];
        snprintf(driver_path, sizeof(driver_path), "%s/device/driver/module/../../..", card_path);
        char *realpath = realpath(driver_path, NULL);
        if (realpath) {
            char *basename = strrchr(realpath, '/');
            if (basename) basename++;
            else basename = realpath;
            info->has_3d = (strstr(basename, "i915") || strstr(basename, "amdgpu") ||
                            strstr(basename, "lima") || strstr(basename, "panfrost") ||
                            strstr(basename, "adreno") || strstr(basename, "freedreno"));
            info->has_gl = info->has_3d;
            free(realpath);
        }
    }

    /* Get max resolution from available modes */
    /* We'll read from the first connector's modes */
    DIR *dir = opendir(card_path);
    if (!dir)
        return false;

    struct dirent *ent;
    while ((ent = readdir(dir))) {
        if (strncmp(ent->d_name, "card", 4) == 0 ||
            strncmp(ent->d_name, "controlD", 8) == 0) {
            continue;
        }
        /* Look for connector directories */
        if (strncmp(ent->d_name, "card", 4) != 0) {
            char connector_path[256];
            snprintf(connector_path, sizeof(connector_path), "%s/%s", card_path, ent->d_name);
            DIR *conn_dir = opendir(connector_path);
            if (conn_dir) {
                struct dirent *conn_ent;
                while ((conn_ent = readdir(conn_dir))) {
                    if (strncmp(conn_ent->d_name, "mode", 4) == 0) {
                        char mode_path[256];
                        snprintf(mode_path, sizeof(mode_path), "%s/%s", connector_path, conn_ent->d_name);
                        FILE *fp = fopen(mode_path, "r");
                        if (fp) {
                            int hdisplay, vdisplay;
                            if (fscanf(fp, "%*s %d %*s %d", &hdisplay, &vdisplay) == 2) {
                                if (hdisplay > info->max_width)
                                    info->max_width = hdisplay;
                                if (vdisplay > info->max_height)
                                    info->max_height = vdisplay;
                            }
                            fclose(fp);
                        }
                    }
                }
                closedir(conn_dir);
            }
        }
    }
    closedir(dir);

    /* If we didn't find any modes, set a default */
    if (info->max_width == 0 || info->max_height == 0) {
        info->max_width = 1920;
        info->max_height = 1080;
    }

    return true;
}

/* Probe for GPU */
bool
gpu_probe(struct gpu_info *info)
{
    if (!info)
        return false;

    memset(info, 0, sizeof(*info));

    /* Try to find a DRM card */
    DIR *dir = opendir("/sys/class/drm");
    if (!dir)
        return false;

    struct dirent *ent;
    while ((ent = readdir(dir))) {
        if (strncmp(ent->d_name, "card", 4) == 0) {
            char card_path[256];
            snprintf(card_path, sizeof(card_path), "/sys/class/drm/%s", ent->d_name);
            if (probe_drm_gpu(info, card_path)) {
                closedir(dir);
                return true;
            }
        }
    }
    closedir(dir);

    /* Fallback: try PCI devices */
    dir = opendir("/sys/bus/pci/devices");
    if (!dir)
        return false;

    while ((ent = readdir(dir))) {
        char device_path[256];
        snprintf(device_path, sizeof(device_path), "/sys/bus/pci/devices/%s", ent->d_name);
        char vendor_path[256];
        snprintf(vendor_path, sizeof(vendor_path), "%s/vendor", device_path);
        char device_id_path[256];
        snprintf(device_id_path, sizeof(device_id_path), "%s/device", device_path);

        int vendor = read_sysfs_int(vendor_path);
        int device = read_sysfs_int(device_id_path);
        if (vendor < 0 || device < 0)
            continue;

        /* Check if it's a known GPU vendor */
        if (vendor == GPU_VENDOR_INTEL || vendor == GPU_VENDOR_AMD ||
            vendor == GPU_VENDOR_ARM || vendor == GPU_VENDOR_QUALCOMM) {
            info->vendor = vendor;
            info->device = device;
            switch (vendor) {
            case GPU_VENDOR_INTEL: info->name = "Intel"; break;
            case GPU_VENDOR_AMD: info->name = "AMD"; break;
            case GPU_VENDOR_ARM: info->name = "ARM"; break;
            case GPU_VENDOR_QUALCOMM: info->name = "Qualcomm"; break;
            }
            /* Assume 3D/GL for known GPUs */
            info->has_3d = true;
            info->has_gl = true;
            info->max_width = 1920;
            info->max_height = 1080;
            closedir(dir);
            return true;
        }
    }
    closedir(dir);

    return false;
}

/* Get GPU name */
const char *
gpu_get_name(struct gpu_info *info)
{
    return info ? info->name : "Unknown";
}