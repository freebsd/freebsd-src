# FreeBSD Native OCI Container Tooling — Implementation Plan

**Author:** Mark LaPointe <mark@cloudbsd.org>  
**Date:** 2026-04-25  
**Repository:** https://github.com/cloudbsdorg/freebsd-src-oci  
**Branch:** `oci-plan`

---

## 1. Executive Summary

This document outlines a sensible, incremental approach to building native FreeBSD tooling for OCI (Open Container Initiative) containers using the operating system's existing jail infrastructure. The goal is to provide a lightweight, native alternative to the `podman-suite` port, leveraging FreeBSD's mature jail system, ZFS integration, and VNET networking to run OCI-compliant containers without the overhead and complexity of a full Linux compatibility layer.

**Primary Recommendation:** Build a native FreeBSD OCI runtime (`ocifbsd`) that translates OCI container specifications into FreeBSD jail configurations, using existing kernel and userland primitives.

---

## 2. Current Architecture Analysis

### 2.1 Components

| Component | Location | Role |
|-----------|----------|------|
| `jail` | `sys/kern/kern_jail.c` | Kernel jail subsystem — process isolation, chroot, virtualization |
| `jail` | `usr.sbin/jail/jail.c` | Userland jail management utility |
| `jexec` | `usr.sbin/jexec/jexec.c` | Execute commands inside a running jail |
| `jls` | `usr.sbin/jls/jls.c` | List active jails and their properties |
| `vnet` | `sys/net/vnet.h` | Virtual network stack for per-jail networking |
| `zfs` | `sys/cddl/contrib/opensolaris/uts/common/fs/zfs/` | ZFS filesystem with native snapshot, clone, and dataset support |
| `oci-image` | `release/Makefile.oci`, `release/scripts/make-oci-image.sh` | Existing OCI image build infrastructure for FreeBSD base images |
| `oci-image-static.conf` | `release/tools/oci-image-static.conf` | Configuration for minimal static-linked base OCI images |
| `oci-image-dynamic.conf` | `release/tools/oci-image-dynamic.conf` | Configuration for dynamic-linked base OCI images |
| `oci-image-runtime.conf` | `release/tools/oci-image-runtime.conf` | Configuration for runtime OCI images |
| `oci-image-notoolchain.conf` | `release/tools/oci-image-notoolchain.conf` | Configuration for images without toolchain |
| `oci-image-toolchain.conf` | `release/tools/oci-image-toolchain.conf` | Configuration for full toolchain images |

### 2.2 Current Jail Capabilities

- **Process isolation:** Separate PID namespace, limited sysctl access
- **Filesystem isolation:** Chroot with optional nullfs mounts, devfs, fdescfs, procfs
- **Network isolation:** VNET for full network stack virtualization, or shared IP with IP aliases
- **Resource limits:** RCTL (Resource Control) for CPU, memory, disk I/O limits
- **ZFS integration:** Per-jail ZFS datasets with quota, snapshot, and clone support
- **Persistent and ephemeral jails:** `persist` parameter for long-running vs. one-shot containers

### 2.3 Existing OCI Image Support

FreeBSD already has infrastructure for building OCI-compliant container images:

```
release/Makefile.oci          — Build targets for OCI images
release/scripts/make-oci-image.sh  — Shell script to construct OCI image layers
release/tools/oci-image-*.conf     — Image configurations (static, dynamic, runtime, etc.)
```

These produce OCI Image Format v1.0.0 compliant tarballs with:
- `oci-layout` — Image layout version
- `index.json` — Image index with manifest references
- `manifest.json` — Image manifest with layer digests
- `config.json` — OCI runtime configuration (architecture, OS, command)
- Layer blobs — gzipped tar archives of rootfs

### 2.4 Gap Analysis

| Capability | Linux (runc/podman) | FreeBSD Native | Gap |
|-----------|---------------------|----------------|-----|
| OCI runtime spec compliance | Full | Partial (images only) | **Runtime translation layer missing** |
| Container lifecycle management | runc/crun | jail + jexec | **Unified CLI missing** |
| Image pull/push | skopeo/podman | None native | **Registry client missing** |
| Image storage | overlayfs | ZFS clone/snapshot | **ZFS-native storage driver needed** |
| Networking (bridge, port fwd) | CNI plugins | ifconfig, pf, ipfw | **CNI-compatible plugin missing** |
| Rootless containers | user namespaces | Not applicable | **Alternative isolation model needed** |
| seccomp/apparmor | seccomp, apparmor | MAC (mac_biba, mac_lomac, mac_mls) | **OCI seccomp → MAC translation needed** |
| cgroups v1/v2 | cgroupfs | RCTL | **RCTL → OCI resources translation needed** |

---

## 3. Proposed Architecture: Native FreeBSD OCI Runtime

### 3.1 High-Level Design

```
+-------------------------------------------------------------+
|                    User Commands                             |
|  ocifbsd run, ocifbsd pull, ocifbsd push, ocifbsd build     |
+----------------------------+--------------------------------+
                             |
                             v
+-------------------------------------------------------------+
|              ocifbsd (OCI Runtime Daemon/CLI)              |
|  - Parses OCI runtime spec and image config                  |
|  - Translates OCI concepts to FreeBSD jail parameters        |
|  - Manages image storage (ZFS datasets)                      |
|  - Handles networking (VNET, bridge, port forwarding)         |
+----------------------------+--------------------------------+
                             |
              +--------------+--------------+
              |                             |
              v                             v
+----------------------------+  +----------------------------+
|      Image Management       |  |    Container Runtime      |
|  - Registry client (pull)   |  |  - jail config generation   |
|  - ZFS snapshot/clone       |  |  - VNET setup               |
|  - Layer caching            |  |  - RCTL resource limits     |
|  - OCI image unpack         |  |  - MAC label assignment       |
+----------------------------+  +----------------------------+
                             |
                             v
+-------------------------------------------------------------+
|              FreeBSD Kernel Primitives                       |
|  jail(2), VNET, ZFS, RCTL, MAC, devfs, nullfs, pf          |
+-------------------------------------------------------------+
```

### 3.2 Key Design Principles

1. **Native first:** Use FreeBSD's existing subsystems (jail, ZFS, VNET, RCTL) rather than emulating Linux
2. **OCI compliant:** Produce and consume standard OCI Image and Runtime specifications
3. **ZFS-native storage:** Leverage ZFS snapshots, clones, and datasets for efficient layer storage
4. **VNET networking:** Use FreeBSD's virtual network stacks for container networking
5. **Incremental deployment:** Start with basic `run` and `pull`, expand to full lifecycle management
6. **Backward compatibility:** Existing jail workflows remain unchanged; new tooling is additive

---

## 4. Implementation Phases

### Phase 1: OCI Runtime Core (`ocifbsd`)

**New directory:** `usr.sbin/ocifbsd/`

#### 4.1.1 Runtime Specification Translation

Create a library that translates OCI Runtime Specification (`config.json`) to FreeBSD jail parameters:

```c
/*
 * OCI Runtime Spec → FreeBSD jail parameter mapping
 * File: usr.sbin/ocifbsd/oci2jail.c
 */

struct oci_runtime_spec {
    struct oci_root        root;        /* Root filesystem path */
    struct oci_process     process;     /* Process configuration (args, env, cwd) */
    struct oci_linux       *linux;      /* Linux-specific (ignored on FreeBSD) */
    struct oci_freebsd     *freebsd;    /* FreeBSD-specific extensions */
    struct oci_hooks       *hooks;      /* Prestart/poststart/poststop hooks */
};

struct jailparam *
oci_spec_to_jail_params(const struct oci_runtime_spec *spec, size_t *nparams)
{
    struct jailparam *params;
    size_t n = 0;
    
    params = emalloc(sizeof(*params) * 32);  /* Initial capacity */
    
    /* Root filesystem */
    jailparam_init(&params[n], "path");
    jailparam_import_raw(&params[n], spec->root.path);
    n++;
    
    /* Hostname */
    if (spec->hostname != NULL) {
        jailparam_init(&params[n], "host.hostname");
        jailparam_import_raw(&params[n], spec->hostname);
        n++;
    }
    
    /* VNET for network isolation */
    if (spec->freebsd != NULL && spec->freebsd->vnet) {
        jailparam_init(&params[n], "vnet");
        jailparam_import_raw(&params[n], "new");
        n++;
    }
    
    /* IP addresses */
    for (int i = 0; i < spec->freebsd->n_ip4; i++) {
        jailparam_init(&params[n], "ip4.addr");
        jailparam_import_raw(&params[n], spec->freebsd->ip4[i]);
        n++;
    }
    
    /* Resource limits via RCTL */
    if (spec->freebsd != NULL && spec->freebsd->rctl != NULL) {
        /* Translate OCI resources to RCTL rules */
        oci_rctl_apply(spec->freebsd->rctl);
    }
    
    /* MAC labels */
    if (spec->freebsd != NULL && spec->freebsd->mac_label != NULL) {
        jailparam_init(&params[n], "allow.chflags");
        jailparam_import_raw(&params[n], "true");
        n++;
        /* Apply MAC label to jail processes */
    }
    
    /* Mount points (devfs, fdescfs, procfs, nullfs) */
    for (int i = 0; i < spec->mounts_len; i++) {
        /* Translate OCI mounts to FreeBSD mount entries */
        oci_mount_to_fstab(&spec->mounts[i]);
    }
    
    *nparams = n;
    return (params);
}
```

#### 4.1.2 Container Lifecycle Management

```c
/*
 * Container lifecycle operations
 * File: usr.sbin/ocifbsd/container.c
 */

struct ocifbsd_container {
    char            *id;            /* Container ID (UUID) */
    char            *name;          /* Human-readable name */
    char            *rootfs;          /* Root filesystem path */
    int             jid;              /* Jail ID */
    pid_t           init_pid;         /* Init process PID */
    struct oci_runtime_spec *spec;    /* OCI runtime spec */
    enum {
        CONTAINER_CREATED,
        CONTAINER_RUNNING,
        CONTAINER_STOPPED,
        CONTAINER_PAUSED,   /* Future: RCTL pause */
    } state;
    time_t          created_at;
    time_t          started_at;
    time_t          finished_at;
    int             exit_code;
};

int
container_create(struct ocifbsd_container **cp, const struct oci_runtime_spec *spec,
    const char *bundle_path)
{
    struct ocifbsd_container *c;
    struct jailparam *params;
    size_t nparams;
    
    c = ecalloc(1, sizeof(*c));
    c->id = generate_container_id();
    c->spec = oci_spec_copy(spec);
    c->state = CONTAINER_CREATED;
    c->created_at = time(NULL);
    
    /* Prepare rootfs (ZFS clone or directory) */
    c->rootfs = prepare_rootfs(bundle_path, spec->root.path, c->id);
    
    /* Generate jail parameters from OCI spec */
    params = oci_spec_to_jail_params(spec, &nparams);
    
    /* Create jail (but don't start process yet) */
    c->jid = jailparam_set(params, nparams, JAIL_CREATE);
    if (c->jid < 0) {
        container_destroy(c);
        return (-1);
    }
    
    *cp = c;
    return (0);
}

int
container_start(struct ocifbsd_container *c)
{
    struct jailparam *params;
    size_t nparams;
    pid_t pid;
    
    if (c->state != CONTAINER_CREATED)
        return (EINVAL);
    
    /* Run prestart hooks */
    oci_hooks_run(c->spec->hooks, "prestart", c->state);
    
    /* Start init process inside jail */
    pid = fork();
    if (pid == 0) {
        /* Child: enter jail and exec process */
        if (jail_attach(c->jid) < 0)
            _exit(126);
        
        /* Set up process environment */
        oci_process_setup(c->spec->process);
        
        /* Exec the container command */
        execvp(c->spec->process->args[0], c->spec->process->args);
        _exit(127);
    }
    
    c->init_pid = pid;
    c->state = CONTAINER_RUNNING;
    c->started_at = time(NULL);
    
    /* Run poststart hooks */
    oci_hooks_run(c->spec->hooks, "poststart", c->state);
    
    return (0);
}

int
container_kill(struct ocifbsd_container *c, int sig)
{
    if (c->state != CONTAINER_RUNNING)
        return (EINVAL);
    
    return (kill(c->init_pid, sig));
}

int
container_delete(struct ocifbsd_container *c)
{
    /* Run poststop hooks */
    oci_hooks_run(c->spec->hooks, "poststop", c->state);
    
    /* Remove jail */
    if (c->jid > 0)
        jail_remove(c->jid);
    
    /* Clean up rootfs (ZFS destroy or rm -rf) */
    cleanup_rootfs(c->rootfs, c->id);
    
    /* Free container structure */
    oci_spec_free(c->spec);
    free(c->id);
    free(c->name);
    free(c->rootfs);
    free(c);
    
    return (0);
}
```

#### 4.1.3 State Directory and Locking

```c
/*
 * Container state persistence
 * File: usr.sbin/ocifbsd/state.c
 */

#define OCIFBSD_STATE_DIR "/var/run/ocifbsd"

int
state_save(const struct ocifbsd_container *c)
{
    char path[PATH_MAX];
    FILE *fp;
    
    snprintf(path, sizeof(path), "%s/%s/state.json", OCIFBSD_STATE_DIR, c->id);
    
    fp = fopen(path, "w");
    if (fp == NULL)
        return (-1);
    
    fprintf(fp, "{\n");
    fprintf(fp, "  \"ociVersion\": \"1.0.2\",\n");
    fprintf(fp, "  \"id\": \"%s\",\n", c->id);
    fprintf(fp, "  \"status\": \"%s\",\n", container_state_string(c->state));
    fprintf(fp, "  \"pid\": %d,\n", c->init_pid);
    fprintf(fp, "  \"jid\": %d,\n", c->jid);
    fprintf(fp, "  \"bundle\": \"%s\",\n", c->rootfs);
    fprintf(fp, "  \"created\": \"%s\"\n", iso8601_time(c->created_at));
    if (c->started_at > 0)
        fprintf(fp, "  \"started\": \"%s\"\n", iso8601_time(c->started_at));
    fprintf(fp, "}\n");
    
    fclose(fp);
    return (0);
}
```

### Phase 2: Image Management and Storage

**New directory:** `usr.sbin/ocifbsd/image/`

#### 4.2.1 ZFS-Based Image Storage

```c
/*
 * ZFS storage driver for OCI images
 * File: usr.sbin/ocifbsd/image/zfs_store.c
 */

#define OCIFBSD_ZFS_POOL "zroot"  /* Configurable */
#define OCIFBSD_ZFS_DATASET "zroot/ocifbsd"

int
zfs_store_init(void)
{
    /* Create base dataset if it doesn't exist */
    zfs_dataset_create(OCIFBSD_ZFS_DATASET, NULL);
    
    /* Create sub-datasets */
    zfs_dataset_create(OCIFBSD_ZFS_DATASET "/images", NULL);
    zfs_dataset_create(OCIFBSD_ZFS_DATASET "/containers", NULL);
    zfs_dataset_create(OCIFBSD_ZFS_DATASET "/layers", NULL);
    
    return (0);
}

char *
zfs_store_create_layer(const char *digest)
{
    char dataset[ZFS_MAX_DATASET_NAME_LEN];
    char mountpoint[PATH_MAX];
    
    snprintf(dataset, sizeof(dataset), "%s/layers/%s", OCIFBSD_ZFS_DATASET, digest);
    snprintf(mountpoint, sizeof(mountpoint), "/var/lib/ocifbsd/layers/%s", digest);
    
    zfs_dataset_create(dataset, mountpoint);
    
    return (strdup(mountpoint));
}

char *
zfs_store_create_container_rootfs(const char *container_id, const char *image_digest)
{
    char dataset[ZFS_MAX_DATASET_NAME_LEN];
    char image_dataset[ZFS_MAX_DATASET_NAME_LEN];
    char mountpoint[PATH_MAX];
    
    /* Clone image dataset for container */
    snprintf(image_dataset, sizeof(image_dataset), "%s/images/%s",
        OCIFBSD_ZFS_DATASET, image_digest);
    snprintf(dataset, sizeof(dataset), "%s/containers/%s",
        OCIFBSD_ZFS_DATASET, container_id);
    snprintf(mountpoint, sizeof(mountpoint), "/var/run/ocifbsd/%s/rootfs",
        container_id);
    
    zfs_dataset_clone(dataset, image_dataset, mountpoint);
    
    return (strdup(mountpoint));
}
```

#### 4.2.2 OCI Image Pull and Unpack

```c
/*
 * Registry client and image unpacking
 * File: usr.sbin/ocifbsd/image/pull.c
 */

int
image_pull(const char *image_name, const char *tag)
{
    struct oci_manifest manifest;
    struct oci_config config;
    char registry_url[256];
    char auth_token[1024];
    
    /* Resolve registry URL from image name */
    registry_resolve(image_name, registry_url, sizeof(registry_url));
    
    /* Authenticate with registry */
    registry_auth(registry_url, image_name, auth_token, sizeof(auth_token));
    
    /* Fetch manifest */
    registry_fetch_manifest(registry_url, image_name, tag, &manifest);
    
    /* Fetch and unpack each layer */
    for (int i = 0; i < manifest.layers_len; i++) {
        struct oci_layer *layer = &manifest.layers[i];
        char *layer_path;
        
        /* Check if layer already exists in local store */
        if (zfs_layer_exists(layer->digest))
            continue;
        
        /* Download layer blob */
        layer_path = zfs_store_create_layer(layer->digest);
        registry_fetch_layer(registry_url, image_name, layer->digest, layer_path);
        
        /* Unpack layer (tar.gz → ZFS dataset) */
        layer_unpack(layer_path, layer->digest);
    }
    
    /* Store image config */
    registry_fetch_config(registry_url, image_name, manifest.config.digest, &config);
    zfs_store_image_config(manifest.config.digest, &config);
    
    /* Tag image */
    image_tag(manifest.config.digest, image_name, tag);
    
    return (0);
}
```

### Phase 3: Networking

**New directory:** `usr.sbin/ocifbsd/network/`

#### 4.3.1 VNET and Bridge Configuration

```c
/*
 * Container networking setup
 * File: usr.sbin/ocifbsd/network/vnet.c
 */

int
network_setup_vnet(struct ocifbsd_container *c, const struct oci_freebsd_network *net)
{
    char bridge_name[IFNAMSIZ];
    char epair_a[IFNAMSIZ], epair_b[IFNAMSIZ];
    int epair_unit;
    
    /* Create epair interface */
    epair_unit = if_create_epair(epair_a, epair_b);
    if (epair_unit < 0)
        return (-1);
    
    /* Add epair_a to bridge (host side) */
    if (net->bridge != NULL) {
        snprintf(bridge_name, sizeof(bridge_name), "bridge%s", net->bridge);
        if_add_to_bridge(epair_a, bridge_name);
    }
    
    /* Configure host-side IP if specified */
    if (net->host_ip != NULL)
        if_configure_ip(epair_a, net->host_ip);
    
    /* Move epair_b to jail (container side) */
    if_move_to_jail(epair_b, c->jid);
    
    /* Configure container-side IP */
    if (net->container_ip != NULL) {
        /* Will be configured inside jail after start */
        c->spec->freebsd->vnet_interface = strdup(epair_b);
    }
    
    /* Set up NAT/port forwarding if requested */
    if (net->port_forward != NULL) {
        for (int i = 0; i < net->n_port_forward; i++) {
            pf_add_redirect(net->port_forward[i].host_port,
                net->port_forward[i].container_port,
                net->container_ip);
        }
    }
    
    return (0);
}
```

#### 4.3.2 CNI-Compatible Plugin Interface

```c
/*
 * CNI-compatible network plugin interface
 * File: usr.sbin/ocifbsd/network/cni.c
 */

int
cni_configure(const char *container_id, const char *netns_path,
    const char *ifname, const struct cni_config *config)
{
    /* Translate CNI config to FreeBSD network setup */
    switch (config->type) {
    case CNI_BRIDGE:
        return (cni_bridge_setup(container_id, netns_path, ifname, config));
    case CNI_HOST_LOCAL:
        return (cni_host_local_setup(container_id, netns_path, ifname, config));
    case CNI_LOOPBACK:
        return (cni_loopback_setup(container_id, netns_path, ifname));
    default:
        return (ENOTSUP);
    }
}
```

### Phase 4: Resource Limits and Security

#### 4.4.1 RCTL Translation

```c
/*
 * OCI resources → FreeBSD RCTL translation
 * File: usr.sbin/ocifbsd/rctl.c
 */

int
oci_rctl_apply(const char *container_id, const struct oci_resources *res)
{
    char rule[RCTL_RULE_MAX];
    
    /* CPU limits */
    if (res->cpu != NULL) {
        if (res->cpu->shares > 0) {
            snprintf(rule, sizeof(rule), "jail:%s:pcpu:deny=%d",
                container_id, res->cpu->shares / 10);  /* Convert shares to % */
            rctl_add_rule(rule);
        }
        if (res->cpu->quota > 0 && res->cpu->period > 0) {
            snprintf(rule, sizeof(rule), "jail:%s:pcpu:deny=%d",
                container_id, (int)(res->cpu->quota * 100 / res->cpu->period));
            rctl_add_rule(rule);
        }
    }
    
    /* Memory limits */
    if (res->memory != NULL) {
        if (res->memory->limit > 0) {
            snprintf(rule, sizeof(rule), "jail:%s:memoryuse:deny=%ld",
                container_id, res->memory->limit);
            rctl_add_rule(rule);
        }
        if (res->memory->swap > 0) {
            snprintf(rule, sizeof(rule), "jail:%s:swapuse:deny=%ld",
                container_id, res->memory->swap);
            rctl_add_rule(rule);
        }
    }
    
    /* Process limits */
    if (res->pids != NULL && res->pids->limit > 0) {
        snprintf(rule, sizeof(rule), "jail:%s:nproc:deny=%ld",
            container_id, res->pids->limit);
        rctl_add_rule(rule);
    }
    
    return (0);
}
```

#### 4.4.2 MAC Label Translation

```c
/*
 * OCI security → FreeBSD MAC translation
 * File: usr.sbin/ocifbsd/mac.c
 */

int
oci_mac_apply(const char *container_id, const struct oci_linux_seccomp *seccomp)
{
    /*
     * FreeBSD doesn't have seccomp. Instead, we can use:
     * - MAC frameworks (mac_biba, mac_lomac, mac_mls)
     * - Capsicum capabilities
     * - Jail restrictions (allow.* parameters)
     */
    
    if (seccomp != NULL && seccomp->default_action == SCMP_ACT_ERRNO) {
        /* Strict mode: use mac_biba with high integrity label */
        mac_set_label(container_id, "biba/high");
        
        /* Restrict jail capabilities */
        jail_set_param(container_id, "allow.raw_sockets", "0");
        jail_set_param(container_id, "allow.chflags", "0");
        jail_set_param(container_id, "allow.mount", "0");
        jail_set_param(container_id, "allow.quotas", "0");
        jail_set_param(container_id, "allow.socket_af", "0");
    }
    
    return (0);
}
```

---

## 5. Alternative Approaches Considered

### 5.1 Port podman to FreeBSD (Rejected)

**Approach:** Continue maintaining the `podman-suite` port with FreeBSD patches.

**Why Rejected:**
- Heavy dependency on Linux compatibility layer (cgroup, seccomp, overlayfs)
- Ongoing maintenance burden for each podman release
- Performance overhead from emulation
- Doesn't leverage FreeBSD's native strengths (ZFS, jails, VNET)

### 5.2 Use Linux KVM/Bhyve for Containers (Rejected)

**Approach:** Run Linux VMs for containers instead of native FreeBSD containers.

**Why Rejected:**
- High overhead per container (full VM)
- Doesn't solve native FreeBSD container needs
- Wastes resources for lightweight workloads
- Breaks OCI "share the host kernel" principle

### 5.3 Extend runc for FreeBSD (Rejected)

**Approach:** Port runc (reference OCI runtime) to FreeBSD.

**Why Rejected:**
- runc is deeply tied to Linux kernel interfaces (cgroups, namespaces, seccomp)
- Would require massive #ifdef proliferation
- FreeBSD's jail model is fundamentally different from Linux namespaces
- Better to design for FreeBSD's strengths than emulate Linux

---

## 6. Tooling and Management Interface

### 6.1 Core CLI: `ocifbsd`

**Files:** `usr.sbin/ocifbsd/ocifbsd.c`, `usr.sbin/ocifbsd/ocifbsd.8`

**Commands:**

```sh
# Image management
ocifbsd pull freebsd:latest                    # Pull OCI image from registry
ocifbsd push freebsd:latest registry.example.com/freebsd:latest  # Push image
ocifbsd images                                 # List local images
ocifbsd rmi freebsd:latest                     # Remove image
ocifbsd build -t myapp:latest .                # Build image from Dockerfile/OCIfile

# Container lifecycle
ocifbsd run -d --name web freebsd:latest nginx  # Run container
ocifbsd ps                                     # List running containers
ocifbsd stop web                               # Stop container
ocifbsd start web                              # Start stopped container
ocifbsd rm web                                 # Remove container
ocifbsd exec web sh                            # Execute command in container
ocifbsd logs web                               # Show container logs

# Low-level OCI runtime
ocifbsd create --bundle /var/lib/ocifbsd/bundles/web web  # Create container
ocifbsd start web                              # Start created container
ocifbsd kill -s TERM web                       # Send signal to container
ocifbsd delete web                             # Delete container
ocifbsd state web                              # Show container state
```

**Modifications:**
1. **Add `ocifbsd` utility** — Main CLI for container management
2. **Add `ocifbsd.8` man page** — Document all commands and options
3. **Add `ocifbsd.conf.5` man page** — Configuration file format

### 6.2 Configuration File

**File:** `etc/ocifbsd/ocifbsd.conf`

```sh
# ocifbsd configuration
storage_driver="zfs"
storage_dataset="zroot/ocifbsd"

# Network defaults
default_bridge="bridge0"
default_network="bridge"

# Registry settings
registry_mirrors="https://docker.io https://ghcr.io"

# Runtime defaults
default_ulimits="nofile=1024:4096"
default_cgroup_parent="ocifbsd"
```

### 6.3 rc.d Script

**Files:** `libexec/rc/rc.d/ocifbsd`, `libexec/rc/rc.conf`

**Modifications:**
1. **Add `ocifbsd_enable` rc.conf variable** — Enable container auto-start (default: `"NO"`)
2. **Add `ocifbsd_containers` rc.conf variable** — List of containers to auto-start
3. **Update startup script** — Start configured containers on boot:
   ```sh
   if checkyesno ocifbsd_enable; then
       for container in ${ocifbsd_containers}; do
           echo "Starting container ${container}..."
           ocifbsd start ${container}
       done
   fi
   ```

---

## 7. Implementation Details

### 7.1 File Changes

| File | Change Type | Description |
|------|-------------|-------------|
| `usr.sbin/ocifbsd/` | New | Main OCI runtime directory |
| `usr.sbin/ocifbsd/Makefile` | New | Build system for ocifbsd |
| `usr.sbin/ocifbsd/ocifbsd.c` | New | Main CLI entry point |
| `usr.sbin/ocifbsd/ocifbsd.h` | New | Common header definitions |
| `usr.sbin/ocifbsd/oci2jail.c` | New | OCI spec → jail parameter translation |
| `usr.sbin/ocifbsd/oci2jail.h` | New | Translation function declarations |
| `usr.sbin/ocifbsd/container.c` | New | Container lifecycle management |
| `usr.sbin/ocifbsd/container.h` | New | Container structure definitions |
| `usr.sbin/ocifbsd/state.c` | New | Container state persistence |
| `usr.sbin/ocifbsd/state.h` | New | State management declarations |
| `usr.sbin/ocifbsd/image/` | New | Image management directory |
| `usr.sbin/ocifbsd/image/zfs_store.c` | New | ZFS storage driver |
| `usr.sbin/ocifbsd/image/pull.c` | New | Image pull from registry |
| `usr.sbin/ocifbsd/image/push.c` | New | Image push to registry |
| `usr.sbin/ocifbsd/image/unpack.c` | New | OCI layer unpacking |
| `usr.sbin/ocifbsd/network/` | New | Networking directory |
| `usr.sbin/ocifbsd/network/vnet.c` | New | VNET setup for containers |
| `usr.sbin/ocifbsd/network/bridge.c` | New | Bridge configuration |
| `usr.sbin/ocifbsd/network/cni.c` | New | CNI-compatible plugin interface |
| `usr.sbin/ocifbsd/rctl.c` | New | OCI resources → RCTL translation |
| `usr.sbin/ocifbsd/mac.c` | New | OCI security → MAC translation |
| `usr.sbin/ocifbsd/hooks.c` | New | OCI hooks execution |
| `usr.sbin/ocifbsd/utils.c` | New | Utility functions |
| `usr.sbin/ocifbsd/ocifbsd.8` | New | Main man page |
| `usr.sbin/ocifbsd/ocifbsd.conf.5` | New | Configuration man page |
| `libexec/rc/rc.d/ocifbsd` | New | Startup script |
| `libexec/rc/rc.conf` | Modify | Add ocifbsd variables |
| `etc/ocifbsd/` | New | Configuration directory |
| `etc/ocifbsd/ocifbsd.conf` | New | Default configuration |
| `sys/kern/kern_jail.c` | Minor | Potential jail parameter extensions |
| `sys/sys/jail.h` | Minor | New jail parameters if needed |
| `usr.sbin/jail/jail.c` | Minor | Compatibility with new parameters |
| `release/Makefile.oci` | Modify | Integrate with ocifbsd image building |
| `release/scripts/make-oci-image.sh` | Modify | Support ocifbsd-specific annotations |

### 7.2 OCI FreeBSD Extensions

To support FreeBSD-specific features, we propose extending the OCI runtime spec with a `freebsd` section:

```json
{
  "ociVersion": "1.0.2",
  "platform": {
    "os": "freebsd",
    "arch": "amd64"
  },
  "root": {
    "path": "rootfs"
  },
  "process": {
    "args": ["nginx", "-g", "daemon off;"]
  },
  "freebsd": {
    "vnet": true,
    "ip4": ["192.168.1.100/24"],
    "ip6": ["2001:db8::100/64"],
    "mac_label": "biba/high",
    "rctl": {
      "memoryuse": "1G",
      "pcpu": "50",
      "nproc": "100"
    },
    "mounts": [
      {
        "destination": "/dev",
        "type": "devfs",
        "source": "devfs",
        "options": ["ruleset=4"]
      }
    ],
    "network": {
      "type": "bridge",
      "bridge": "bridge0",
      "port_forward": [
        {
          "host_port": 8080,
          "container_port": 80,
          "protocol": "tcp"
        }
      ]
    }
  }
}
```

### 7.3 sysctl Variables

| Variable | Type | Default | Description |
|----------|------|---------|-------------|
| `kern.ocifbsd.enabled` | int | 1 | Global enable/disable for ocifbsd features |
| `kern.ocifbsd.max_containers` | int | 0 | Maximum containers (0=unlimited) |
| `kern.ocifbsd.default_vnet` | int | 1 | Default VNET enable for new containers |
| `kern.ocifbsd.zfs_pool` | string | "zroot" | Default ZFS pool for storage |

---

## 8. Testing Strategy and Isolated Test Harnesses

**Critical Requirement:** All testing must be performed in isolation. The test harnesses must never load experimental kernel code into the host operating system. Use VMs, user-mode simulation, or dedicated test hardware.

### 8.1 Testing Philosophy

| Level | Environment | Purpose |
|-------|-------------|---------|
| Unit Tests | User-mode mock framework | Test logic in isolation |
| Integration Tests | VM with snapshot rollback | Test jail creation, networking, ZFS operations |
| Performance Tests | Dedicated test hardware or VM with passthrough | Measure container startup time, resource overhead |
| Stress Tests | VM with resource limits | Verify stability and memory safety |

### 8.2 Unit Test Harness: `tests/usr.sbin/ocifbsd/ocifbsd_test.c`

**File:** `tests/usr.sbin/ocifbsd/ocifbsd_test.c`

**Approach:** Create a user-mode test framework that mocks jail, ZFS, and network APIs. This allows testing the OCI translation logic without loading any kernel code.

```c
/*
 * User-mode unit test for OCI spec translation.
 * This test runs entirely in userland and does not load any kernel modules.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

/* Mock kernel structures */
struct mock_jailparam {
    char name[64];
    char value[256];
};

struct mock_oci_spec {
    char root_path[PATH_MAX];
    char hostname[64];
    int vnet;
    char ip4[4][32];
    int n_ip4;
};

/* Include the translation logic (extracted into a testable unit) */
#include "oci2jail_logic.h"

static void
test_basic_translation(void)
{
    struct mock_oci_spec spec = {
        .root_path = "/var/run/ocifbsd/test/rootfs",
        .hostname = "testcontainer",
        .vnet = 1,
        .n_ip4 = 1,
    };
    strcpy(spec.ip4[0], "192.168.1.100/24");
    
    struct mock_jailparam params[16];
    size_t nparams;
    
    nparams = oci_spec_to_jail_params_mock(&spec, params, 16);
    
    assert(nparams >= 3);  /* path, hostname, vnet */
    assert(strcmp(params[0].name, "path") == 0);
    assert(strcmp(params[0].value, "/var/run/ocifbsd/test/rootfs") == 0);
    
    printf("PASS: basic translation\n");
}

static void
test_vnet_disabled(void)
{
    struct mock_oci_spec spec = {
        .root_path = "/var/run/ocifbsd/test/rootfs",
        .vnet = 0,
        .n_ip4 = 0,
    };
    
    struct mock_jailparam params[16];
    size_t nparams;
    
    nparams = oci_spec_to_jail_params_mock(&spec, params, 16);
    
    /* Should not include vnet parameter */
    for (size_t i = 0; i < nparams; i++) {
        assert(strcmp(params[i].name, "vnet") != 0);
    }
    
    printf("PASS: vnet disabled\n");
}

int
main(int argc, char **argv)
{
    printf("ocifbsd unit tests (user-mode, no kernel code)\n");
    test_basic_translation();
    test_vnet_disabled();
    printf("All tests passed.\n");
    return (0);
}
```

**Build and run:**
```sh
cd tests/usr.sbin/ocifbsd
make ocifbsd_test
./ocifbsd_test
```

### 8.3 Integration Test Harness: VM-Based Testing

**File:** `tests/usr.sbin/ocifbsd/ocifbsd_vm_test.sh`

**Approach:** Use a virtual machine (bhyve, QEMU) with snapshot support. The test script:
1. Starts a fresh VM from a snapshot.
2. Copies the built ocifbsd binary into the VM.
3. Runs container operations inside the VM.
4. Verifies expected behavior.
5. Reverts the VM to the snapshot, leaving no trace.

```sh
#!/bin/sh
#
# VM-based integration test for ocifbsd
# This script NEVER loads experimental code on the host.
#

set -e

VM_NAME="ocifbsd-test"
VM_SNAPSHOT="clean"
SSH_PORT="2222"
TEST_RESULTS="/tmp/ocifbsd_test_results.log"

# Revert VM to clean snapshot
echo "Reverting VM to clean snapshot..."
vm revert "${VM_NAME}" "${VM_SNAPSHOT}"

# Start VM
echo "Starting test VM..."
vm start "${VM_NAME}"
sleep 10  # Wait for VM to boot

# Copy ocifbsd binary into VM
echo "Copying ocifbsd to VM..."
scp -P "${SSH_PORT}" usr.sbin/ocifbsd/ocifbsd \
    root@localhost:/usr/local/bin/

# Run tests inside VM via SSH
echo "Running integration tests inside VM..."
ssh -p "${SSH_PORT}" root@localhost << 'TESTSCRIPT'
    set -e
    echo "=== Creating test OCI bundle ==="
    mkdir -p /tmp/test-bundle/rootfs
    
    echo "=== Creating OCI config ==="
    cat > /tmp/test-bundle/config.json <<'EOF'
    {
      "ociVersion": "1.0.2",
      "root": { "path": "rootfs" },
      "process": { "args": ["/bin/sh", "-c", "echo hello from container"] }
    }
    EOF
    
    echo "=== Creating container ==="
    ocifbsd create --bundle /tmp/test-bundle test-container
    
    echo "=== Starting container ==="
    ocifbsd start test-container
    
    echo "=== Checking container state ==="
    ocifbsd state test-container | grep -q "running"
    
    echo "=== Stopping container ==="
    ocifbsd kill -s TERM test-container
    sleep 2
    
    echo "=== Deleting container ==="
    ocifbsd delete test-container
    
    echo "All integration tests passed."
TESTSCRIPT

# Stop VM
echo "Stopping test VM..."
vm stop "${VM_NAME}"

echo "Integration tests completed successfully."
```

### 8.4 Performance Test Harness

**File:** `tests/usr.sbin/ocifbsd/ocifbsd_perf.c`

**Approach:** Run inside a VM with dedicated vCPUs. Measure container startup time, memory overhead, and throughput.

```c
/*
 * Performance test for ocifbsd.
 * Runs inside a VM. Measures container lifecycle performance.
 */

#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define NUM_CONTAINERS 100

int
main(int argc, char **argv)
{
    struct timespec start, end;
    double elapsed;
    int i;
    
    printf("Performance test: %d container create/start/stop/delete cycles\n",
        NUM_CONTAINERS);
    
    clock_gettime(CLOCK_MONOTONIC, &start);
    
    for (i = 0; i < NUM_CONTAINERS; i++) {
        char id[32];
        snprintf(id, sizeof(id), "perf-test-%d", i);
        
        /* Create container */
        systemf("ocifbsd create --bundle /tmp/perf-bundle %s", id);
        
        /* Start container */
        systemf("ocifbsd start %s", id);
        
        /* Stop container */
        systemf("ocifbsd kill -s TERM %s", id);
        sleep(1);
        
        /* Delete container */
        systemf("ocifbsd delete %s", id);
    }
    
    clock_gettime(CLOCK_MONOTONIC, &end);
    
    elapsed = (end.tv_sec - start.tv_sec) +
              (end.tv_nsec - start.tv_nsec) / 1e9;
    
    printf("Containers: %d\n", NUM_CONTAINERS);
    printf("Total time: %.3f seconds\n", elapsed);
    printf("Average per container: %.3f seconds\n", elapsed / NUM_CONTAINERS);
    printf("Throughput: %.1f containers/sec\n", NUM_CONTAINERS / elapsed);
    
    return (0);
}
```

### 8.5 Stress Test Harness

**File:** `tests/usr.sbin/ocifbsd/ocifbsd_stress.sh`

**Approach:** Run inside a VM with memory and CPU limits. Rapidly create and destroy containers while monitoring for leaks and crashes.

```sh
#!/bin/sh
#
# Stress test for ocifbsd
# Runs inside a VM with resource limits.
#

set -e

DURATION=300  # 5 minutes
CONCURRENT=20

echo "Starting ${DURATION}s stress test with ${CONCURRENT} concurrent containers..."

# Monitor memory usage in background
(
    while true; do
        vmstat -m | grep jail >> /tmp/ocifbsd_memory.log
        sleep 1
    done
) &
MONITOR_PID=$!

# Run stress test
start=$(date +%s)
while [ $(($(date +%s) - start)) -lt ${DURATION} ]; do
    for i in $(seq 1 ${CONCURRENT}); do
        id="stress-$(date +%s)-${i}"
        ocifbsd create --bundle /tmp/stress-bundle ${id}
        ocifbsd start ${id}
    done
    
    sleep 5
    
    for i in $(seq 1 ${CONCURRENT}); do
        id="stress-$(date +%s)-${i}"
        ocifbsd kill -s TERM ${id} 2>/dev/null || true
        ocifbsd delete ${id} 2>/dev/null || true
    done
done

kill $MONITOR_PID

echo "Stress test completed."
echo "Memory usage log: /tmp/ocifbsd_memory.log"

# Check for memory leaks
if grep -q "fail" /tmp/ocifbsd_memory.log; then
    echo "FAIL: Memory allocation failures detected"
    exit 1
fi

echo "PASS: No memory issues detected"
```

### 8.6 Test Summary

| Test Type | Location | Environment | Kernel Code Loaded? |
|-----------|----------|-------------|---------------------|
| Unit Tests | `tests/usr.sbin/ocifbsd/ocifbsd_test.c` | User-mode | **No** |
| Integration Tests | `tests/usr.sbin/ocifbsd/ocifbsd_vm_test.sh` | VM | Yes, inside VM only |
| Performance Tests | `tests/usr.sbin/ocifbsd/ocifbsd_perf.c` | VM with dedicated vCPUs | Yes, inside VM only |
| Stress Tests | `tests/usr.sbin/ocifbsd/ocifbsd_stress.sh` | VM with resource limits | Yes, inside VM only |

**Safety Rules:**
1. Never run experimental jail code on the host system.
2. Always use VM snapshots so tests start from a known clean state.
3. Automated CI must use VMs or containers with kernel isolation.
4. Physical test hardware should be dedicated lab equipment, not production servers.

---

## 9. Testing Strategy

### 9.1 Functional Testing

1. **Container creation:** Verify OCI spec → jail parameter translation
2. **Container lifecycle:** Create, start, stop, delete sequence
3. **Image pull:** Download and unpack OCI images from registry
4. **Image storage:** Verify ZFS snapshot/clone behavior
5. **Networking:** VNET creation, bridge attachment, IP assignment
6. **Resource limits:** RCTL rule application and enforcement
7. **Hooks:** Prestart, poststart, poststop hook execution
8. **State persistence:** Container state JSON correctness

### 9.2 Performance Testing

1. **Startup time:** Measure container create+start latency
2. **Memory overhead:** Compare jail vs. VM memory usage
3. **Throughput:** Container operations per second
4. **Scalability:** Maximum containers per host
5. **ZFS efficiency:** Snapshot/clone performance vs. copy

### 9.3 Stress Testing

1. **Rapid create/destroy:** Verify no leaks in jail IDs or ZFS datasets
2. **Concurrent containers:** Run many containers simultaneously
3. **Resource exhaustion:** Verify graceful handling of RCTL denials
4. **Network stress:** High packet rates through VNET interfaces
5. **ZFS pressure:** Many snapshots/clones on same pool

---

## 10. Risks and Mitigations

| Risk | Impact | Mitigation |
|------|--------|------------|
| Jail parameter incompatibility | High | Extensive testing with all jail parameter combinations |
| ZFS dataset proliferation | Medium | Implement automatic cleanup, garbage collection |
| VNET performance overhead | Medium | Benchmark vs. shared IP; optimize if needed |
| Registry API changes | Low | Use stable OCI Distribution Spec v1.0 |
| Backward compatibility | Low | Existing jail workflows unchanged; new tooling is additive |
| Security model differences | Medium | Document FreeBSD MAC vs. Linux seccomp mapping |
| Rootless containers | Medium | Phase 2: investigate unprivileged jail extensions |
| OCI spec compliance | High | Run OCI conformance tests, validate with `oci-runtime-tools` |

---

## 11. TODO — Step-by-Step Implementation Tracker

This section is the master checklist for implementing FreeBSD native OCI tooling. Each task includes:
- **Status:** `NOT STARTED` | `IN PROGRESS` | `COMPLETED`
- **Owner:** Who is working on it
- **Start Date:** When work began
- **End Date:** When work finished
- **Dependencies:** What must be done first
- **Files Modified:** What files are touched
- **Notes:** Any blockers, decisions, or context

### Phase 0: Foundation and Setup

| # | Task | Status | Owner | Start | End | Dependencies | Files | Notes |
|---|------|--------|-------|-------|-----|--------------|-------|-------|
| 0.1 | Create feature branch `feature/oci-jail-native` | NOT STARTED | | | | | | Branch from `main` |
| 0.2 | Set up VM test environment (bhyve/QEMU) | NOT STARTED | | | | | | Must support ZFS, VNET, snapshot rollback |
| 0.3 | Verify existing jail tests pass on clean branch | NOT STARTED | | | | 0.2 | | Baseline before any changes |
| 0.4 | Document baseline performance (jail creation time) | NOT STARTED | | | | 0.3 | | Seconds per jail, memory overhead |
| 0.5 | Review OCI Runtime Spec v1.0.2 | NOT STARTED | | | | | | Ensure compliance target |
| 0.6 | Review OCI Image Spec v1.0.0 | NOT STARTED | | | | | | Ensure image format compliance |

### Phase 1: OCI Runtime Core (`ocifbsd`)

| # | Task | Status | Owner | Start | End | Dependencies | Files | Notes |
|---|------|--------|-------|-------|-----|--------------|-------|-------|
| 1.1 | Create `usr.sbin/ocifbsd/` directory structure | NOT STARTED | | | | 0.1 | | Makefile, headers, subdirs |
| 1.2 | Create `ocifbsd.h` common header | NOT STARTED | | | | 1.1 | `ocifbsd.h` | Container ID, state enums, constants |
| 1.3 | Create `oci2jail.c` — OCI spec parser | NOT STARTED | | | | 1.2 | `oci2jail.c`, `oci2jail.h` | Parse config.json into structures |
| 1.4 | Create `oci2jail.c` — jail parameter translation | NOT STARTED | | | | 1.3 | `oci2jail.c` | Map OCI fields to jailparam |
| 1.5 | Create `container.c` — container_create() | NOT STARTED | | | | 1.4 | `container.c`, `container.h` | Create jail from OCI spec |
| 1.6 | Create `container.c` — container_start() | NOT STARTED | | | | 1.5 | `container.c` | Fork, jail_attach, exec |
| 1.7 | Create `container.c` — container_kill() | NOT STARTED | | | | 1.6 | `container.c` | Signal delivery to init process |
| 1.8 | Create `container.c` — container_delete() | NOT STARTED | | | | 1.7 | `container.c` | jail_remove, cleanup rootfs |
| 1.9 | Create `state.c` — state persistence | NOT STARTED | | | | 1.5 | `state.c`, `state.h` | JSON state in /var/run/ocifbsd |
| 1.10 | Create `hooks.c` — OCI hooks execution | NOT STARTED | | | | 1.6 | `hooks.c`, `hooks.h` | Prestart, poststart, poststop |
| 1.11 | Create `utils.c` — utility functions | NOT STARTED | | | | 1.1 | `utils.c`, `utils.h` | UUID generation, path helpers |
| 1.12 | Create `ocifbsd.c` — CLI entry point | NOT STARTED | | | | 1.11 | `ocifbsd.c` | Command dispatch |
| 1.13 | Implement `ocifbsd create` command | NOT STARTED | | | | 1.12 | `ocifbsd.c` | Bundle path, container ID |
| 1.14 | Implement `ocifbsd start` command | NOT STARTED | | | | 1.13 | `ocifbsd.c` | Start created container |
| 1.15 | Implement `ocifbsd kill` command | NOT STARTED | | | | 1.14 | `ocifbsd.c` | Signal option |
| 1.16 | Implement `ocifbsd delete` command | NOT STARTED | | | | 1.15 | `ocifbsd.c` | Force option |
| 1.17 | Implement `ocifbsd state` command | NOT STARTED | | | | 1.9 | `ocifbsd.c` | Output state JSON |
| 1.18 | Create `ocifbsd.8` man page | NOT STARTED | | | | 1.17 | `ocifbsd.8` | Document all commands |
| 1.19 | Write unit tests (user-mode mock) | NOT STARTED | | | | 1.4 | `tests/usr.sbin/ocifbsd/ocifbsd_test.c` | No kernel code loaded |
| 1.20 | Run unit tests | NOT STARTED | | | | 1.19 | | Must pass before proceeding |
| 1.21 | Write integration test script (VM-based) | NOT STARTED | | | | 1.18 | `tests/usr.sbin/ocifbsd/ocifbsd_vm_test.sh` | Loads code inside VM only |
| 1.22 | Run integration tests | NOT STARTED | | | | 1.21 | | Verify lifecycle operations |
| 1.23 | Write performance test harness | NOT STARTED | | | | 1.18 | `tests/usr.sbin/ocifbsd/ocifbsd_perf.c` | VM with dedicated vCPUs |
| 1.24 | Run performance tests | NOT STARTED | | | | 1.23 | | Measure startup time |
| 1.25 | Write stress test harness | NOT STARTED | | | | 1.18 | `tests/usr.sbin/ocifbsd/ocifbsd_stress.sh` | VM with resource limits |
| 1.26 | Run stress tests | NOT STARTED | | | | 1.25 | | Check for leaks |
| 1.27 | Code review and cleanup | NOT STARTED | | | | 1.26 | | Style, comments, locking |

### Phase 2: Image Management

| # | Task | Status | Owner | Start | End | Dependencies | Files | Notes |
|---|------|--------|-------|-------|-----|--------------|-------|-------|
| 2.1 | Create `usr.sbin/ocifbsd/image/` directory | NOT STARTED | | | | 1.1 | | Image management module |
| 2.2 | Create `zfs_store.c` — ZFS dataset operations | NOT STARTED | | | | 2.1 | `zfs_store.c`, `zfs_store.h` | Create, clone, destroy datasets |
| 2.3 | Create `pull.c` — registry client | NOT STARTED | | | | 2.2 | `pull.c`, `pull.h` | HTTP client, auth, manifest fetch |
| 2.4 | Create `unpack.c` — layer unpacking | NOT STARTED | | | | 2.3 | `unpack.c`, `unpack.h` | tar.gz extraction to ZFS |
| 2.5 | Create `push.c` — image push | NOT STARTED | | | | 2.4 | `push.c`, `push.h` | Upload layers, manifests |
| 2.6 | Implement `ocifbsd pull` command | NOT STARTED | | | | 2.3 | `ocifbsd.c` | Image name, tag, registry |
| 2.7 | Implement `ocifbsd push` command | NOT STARTED | | | | 2.5 | `ocifbsd.c` | Image name, destination |
| 2.8 | Implement `ocifbsd images` command | NOT STARTED | | | | 2.2 | `ocifbsd.c` | List ZFS datasets |
| 2.9 | Implement `ocifbsd rmi` command | NOT STARTED | | | | 2.8 | `ocifbsd.c` | Remove image, cleanup layers |
| 2.10 | Implement image caching | NOT STARTED | | | | 2.4 | `zfs_store.c` | Layer deduplication via ZFS |
| 2.11 | Test image pull from Docker Hub | NOT STARTED | | | | 2.6 | | Verify FreeBSD base image |
| 2.12 | Test image push to private registry | NOT STARTED | | | | 2.7 | | Verify round-trip |

### Phase 3: Networking

| # | Task | Status | Owner | Start | End | Dependencies | Files | Notes |
|---|------|--------|-------|-------|-----|--------------|-------|-------|
| 3.1 | Create `usr.sbin/ocifbsd/network/` directory | NOT STARTED | | | | 1.1 | | Networking module |
| 3.2 | Create `vnet.c` — VNET setup | NOT STARTED | | | | 3.1 | `vnet.c`, `vnet.h` | Create VNET, assign interfaces |
| 3.3 | Create `bridge.c` — bridge configuration | NOT STARTED | | | | 3.2 | `bridge.c`, `bridge.h` | ifbridge setup |
| 3.4 | Create `cni.c` — CNI plugin interface | NOT STARTED | | | | 3.3 | `cni.c`, `cni.h` | CNI config parsing, plugin exec |
| 3.5 | Implement `ocifbsd network create` | NOT STARTED | | | | 3.4 | `ocifbsd.c` | Bridge, subnet, gateway |
| 3.6 | Implement `ocifbsd network ls` | NOT STARTED | | | | 3.5 | `ocifbsd.c` | List networks |
| 3.7 | Implement `ocifbsd network rm` | NOT STARTED | | | | 3.6 | `ocifbsd.c` | Remove bridge |
| 3.8 | Test VNET container networking | NOT STARTED | | | | 3.2 | | Ping, TCP, UDP |
| 3.9 | Test port forwarding | NOT STARTED | | | | 3.3 | | pf NAT rules |
| 3.10 | Test CNI plugin compatibility | NOT STARTED | | | | 3.4 | | Bridge plugin |

### Phase 4: Resource Limits and Security

| # | Task | Status | Owner | Start | End | Dependencies | Files | Notes |
|---|------|--------|-------|-------|-----|--------------|-------|-------|
| 4.1 | Create `rctl.c` — OCI resources → RCTL | NOT STARTED | | | | 1.1 | `rctl.c`, `rctl.h` | CPU, memory, pids translation |
| 4.2 | Create `mac.c` — OCI security → MAC | NOT STARTED | | | | 1.1 | `mac.c`, `mac.h` | seccomp → MAC labels |
| 4.3 | Implement RCTL rule application | NOT STARTED | | | | 4.1 | `rctl.c` | Apply on container start |
| 4.4 | Implement RCTL rule cleanup | NOT STARTED | | | | 4.3 | `rctl.c` | Remove on container delete |
| 4.5 | Implement MAC label application | NOT STARTED | | | | 4.2 | `mac.c` | Apply on container start |
| 4.6 | Test CPU limits | NOT STARTED | | | | 4.3 | | Verify pcpu enforcement |
| 4.7 | Test memory limits | NOT STARTED | | | | 4.3 | | Verify memoryuse enforcement |
| 4.8 | Test MAC labels | NOT STARTED | | | | 4.5 | | Verify mac_biba enforcement |

### Phase 5: Integration and Polish

| # | Task | Status | Owner | Start | End | Dependencies | Files | Notes |
|---|------|--------|-------|-------|-----|--------------|-------|-------|
| 5.1 | Create `libexec/rc/rc.d/ocifbsd` | NOT STARTED | | | | 1.18 | `rc.d/ocifbsd` | Auto-start containers |
| 5.2 | Update `libexec/rc/rc.conf` | NOT STARTED | | | | 5.1 | `rc.conf` | Add ocifbsd variables |
| 5.3 | Create `etc/ocifbsd/ocifbsd.conf` | NOT STARTED | | | | 5.2 | `ocifbsd.conf` | Default configuration |
| 5.4 | Create `ocifbsd.conf.5` man page | NOT STARTED | | | | 5.3 | `ocifbsd.conf.5` | Document config options |
| 5.5 | Integrate with `release/Makefile.oci` | NOT STARTED | | | | 2.10 | `Makefile.oci` | Build ocifbsd base images |
| 5.6 | Run OCI conformance tests | NOT STARTED | | | | 1.27 | | Validate spec compliance |
| 5.7 | Performance benchmark vs. podman | NOT STARTED | | | | 1.24 | | Startup time, memory |
| 5.8 | Documentation: README.md | NOT STARTED | | | | 5.6 | | User guide |
| 5.9 | Documentation: Architecture.md | NOT STARTED | | | | 5.8 | | Design decisions |
| 5.10 | Final code review | NOT STARTED | | | | 5.9 | | Style, security |
| 5.11 | Submit PR to freebsd-src | NOT STARTED | | | | 5.10 | | Target: main branch |

---

## 12. Future Enhancements

1. **Rootless containers:** Investigate unprivileged jail extensions or Capsicum-based isolation
2. **Container orchestration:** Kubernetes CRI shim for FreeBSD native containers
3. **Live migration:** ZFS send/recv for moving running containers between hosts
4. **DTrace integration:** Container-aware DTrace probes for observability
5. **Jail orchestration:** Docker Compose equivalent for FreeBSD jail stacks
6. **eBPF-based networking:** Explore eBPF for container network policies
7. **ZFS encryption:** Per-container encrypted datasets
8. **Checkpoint/restore:** Save and restore container state (requires jail checkpoint support)

---

## 13. Conclusion

The recommended approach of building a native FreeBSD OCI runtime (`ocifbsd`) provides:

- **Native performance:** Leverages FreeBSD's jail, ZFS, and VNET without emulation overhead
- **OCI compliance:** Produces and consumes standard OCI Image and Runtime specifications
- **ZFS-native storage:** Efficient layer storage via snapshots and clones
- **VNET networking:** Full network stack virtualization per container
- **Incremental deployment:** Start with basic `run` and `pull`, expand to full lifecycle management
- **Backward compatibility:** Existing jail workflows remain unchanged; new tooling is additive

This plan provides a clear roadmap for implementing native FreeBSD container tooling that eliminates the need for the `podman-suite` port while maintaining compatibility with the broader OCI ecosystem.
