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

### 6.4 Detailed CLI Command Reference

This section provides exhaustive documentation for every `ocifbsd` command, its flags, options, and output formats. The design mimics Docker/Podman command structures for familiarity while remaining native to FreeBSD.

#### 6.4.1 Global Flags

These flags apply to all `ocifbsd` commands:

| Flag | Long Form | Description | Default |
|------|-----------|-------------|---------|
| `-D` | `--debug` | Enable debug output | `false` |
| `-c` | `--config` | Path to configuration file | `/etc/ocifbsd/ocifbsd.conf` |
| `-H` | `--host` | Daemon socket path (for client/server mode) | `/var/run/ocifbsd.sock` |
| `-v` | `--version` | Print version and exit | — |
| `-h` | `--help` | Show help for any command | — |
| `-q` | `--quiet` | Suppress non-error output | `false` |
| `-J` | `--format json` | Output in JSON format | `table` |

#### 6.4.2 Image Management Commands

**`ocifbsd pull [OPTIONS] NAME[:TAG|@DIGEST]`**

Download an OCI image from a registry.

| Flag | Description | Example |
|------|-------------|---------|
| `-a`, `--arch` | Architecture to pull | `--arch amd64` |
| `-o`, `--os` | OS to pull | `--os freebsd` |
| `--platform` | Platform specifier (`os/arch`) | `--platform freebsd/amd64` |
| `--tls-verify` | Require TLS verification | `true` |
| `--authfile` | Path to authentication file | `~/.ocifbsd/auth.json` |
| `--retry` | Number of retries on failure | `3` |
| `--retry-delay` | Delay between retries (seconds) | `5` |

```sh
# Pull latest FreeBSD base image
ocifbsd pull freebsd:latest

# Pull specific version with explicit registry
ocifbsd pull docker.io/freebsd:14.0

# Pull by digest for reproducible builds
ocifbsd pull freebsd@sha256:abc123...

# Pull for different architecture
ocifbsd pull --arch arm64 freebsd:latest
```

**`ocifbsd push [OPTIONS] NAME[:TAG] [DESTINATION]`**

Upload an OCI image to a registry.

| Flag | Description | Default |
|------|-------------|---------|
| `--tls-verify` | Require TLS verification | `true` |
| `--authfile` | Path to authentication file | `~/.ocifbsd/auth.json` |
| `--format` | Image format (`oci`, `v2s2`) | `oci` |
| `--compression` | Layer compression (`gzip`, `zstd`, `none`) | `gzip` |
| `--remove-signatures` | Do not copy signatures | `false` |

```sh
# Push local image to registry
ocifbsd push myapp:latest registry.example.com/myapp:latest

# Push with zstd compression
ocifbsd push --compression zstd myapp:latest ghcr.io/user/myapp:v1.0
```

**`ocifbsd images [OPTIONS]`**

List locally stored images.

| Flag | Description | Default |
|------|-------------|---------|
| `-a`, `--all` | Show intermediate layers | `false` |
| `-f`, `--filter` | Filter output | — |
| `--format` | Custom output format | `table` |
| `--noheading` | Omit column headers | `false` |
| `--sort` | Sort by column | `created` |
| `-n`, `--names-only` | Show only image names | `false` |

```sh
# List all images
ocifbsd images

# Output as JSON
ocifbsd images --format json

# Filter by name
ocifbsd images --filter "name=freebsd*"

# Show only image names
ocifbsd images -n
```

**Output format (`ocifbsd images`):**

```
REPOSITORY          TAG       IMAGE ID       CREATED        SIZE
freebsd             latest    a1b2c3d4e5f6   2 weeks ago    450MB
myapp               v1.0      b2c3d4e5f6a7   3 days ago     120MB
nginx               alpine    c3d4e5f6a7b8   1 week ago     85MB
```

**`ocifbsd rmi [OPTIONS] IMAGE [IMAGE...]`**

Remove one or more images.

| Flag | Description | Default |
|------|-------------|---------|
| `-f`, `--force` | Force removal | `false` |
| `-a`, `--all` | Remove all images | `false` |
| `--prune` | Remove unused images | `false` |

```sh
# Remove single image
ocifbsd rmi freebsd:latest

# Force remove image (even if containers use it)
ocifbsd rmi -f myapp:v1.0

# Remove all unused images
ocifbsd rmi --prune
```

**`ocifbsd tag SOURCE_IMAGE[:TAG] TARGET_IMAGE[:TAG]`**

Create a tag referencing an existing image.

```sh
# Tag local image for registry push
ocifbsd tag myapp:latest registry.example.com/myapp:v1.0

# Create additional local tag
ocifbsd tag freebsd:14.0 freebsd:stable
```

**`ocifbsd save [OPTIONS] IMAGE [IMAGE...]`**

Export image(s) to a tar archive.

| Flag | Description | Default |
|------|-------------|---------|
| `-o`, `--output` | Output file (default: stdout) | — |
| `--format` | Archive format (`oci-archive`, `docker-archive`) | `oci-archive` |

```sh
# Save image to file
ocifbsd save -o freebsd-14.tar freebsd:14.0

# Save multiple images
ocifbsd save -o backup.tar freebsd:latest myapp:v1.0
```

**`ocifbsd load [OPTIONS]`**

Load image(s) from a tar archive.

| Flag | Description | Default |
|------|-------------|---------|
| `-i`, `--input` | Input file (default: stdin) | — |
| `-q`, `--quiet` | Suppress output | `false` |

```sh
# Load from file
ocifbsd load -i freebsd-14.tar

# Load from stdin
ocifbsd load < backup.tar
```

**`ocifbsd inspect [OPTIONS] IMAGE`**

Display detailed information about an image.

| Flag | Description | Default |
|------|-------------|---------|
| `-f`, `--format` | Format output using Go template | — |
| `--raw` | Output raw manifest/config JSON | `false` |

```sh
# Inspect image
ocifbsd inspect freebsd:latest

# Get specific field
ocifbsd inspect -f '{{.Config.Cmd}}' freebsd:latest

# Raw config output
ocifbsd inspect --raw freebsd:latest
```

#### 6.4.3 Container Lifecycle Commands

**`ocifbsd run [OPTIONS] IMAGE [COMMAND] [ARG...]`**

Create and start a new container. This is the primary command users will interact with.

| Flag | Description | Example |
|------|-------------|---------|
| `-d`, `--detach` | Run in background | — |
| `--name` | Assign container name | `--name web` |
| `-e`, `--env` | Set environment variable | `-e DEBUG=1` |
| `--env-file` | Read env vars from file | `--env-file .env` |
| `-p`, `--publish` | Publish container port | `-p 8080:80/tcp` |
| `-P`, `--publish-all` | Publish all exposed ports | — |
| `-v`, `--volume` | Bind mount volume | `-v /host:/container` |
| `--mount` | Advanced mount specification | — |
| `-w`, `--workdir` | Working directory | `-w /app` |
| `-u`, `--user` | User to run as | `-u www:www` |
| `--hostname` | Container hostname | `--hostname web01` |
| `--network` | Connect to network | `--network bridge0` |
| `--ip` | Static IP address | `--ip 192.168.1.100` |
| `--mac-address` | MAC address | — |
| `--dns` | DNS server | `--dns 8.8.8.8` |
| `--dns-search` | DNS search domain | — |
| `--restart` | Restart policy (`no`, `on-failure`, `always`, `unless-stopped`) | `no` |
| `--rm` | Remove container on exit | `false` |
| `--read-only` | Mount rootfs read-only | `false` |
| `--privileged` | Grant extended privileges | `false` |
| `--cap-add` | Add capability | — |
| `--cap-drop` | Drop capability | — |
| `--security-opt` | Security options | — |
| `--memory` | Memory limit | `--memory 512M` |
| `--memory-swap` | Swap limit | — |
| `--cpus` | CPU limit | `--cpus 2.0` |
| `--pids-limit` | PID limit | — |
| `--device` | Add host device | `--device /dev/bpf` |
| `--log-driver` | Logging driver | `json-file` |
| `--log-opt` | Log driver options | — |
| `--health-cmd` | Health check command | — |
| `--health-interval` | Health check interval | `30s` |
| `--health-timeout` | Health check timeout | `30s` |
| `--health-retries` | Health check retries | `3` |
| `--label` | Set metadata label | `--label env=prod` |
| `--label-file` | Read labels from file | — |
| `-t`, `--tty` | Allocate pseudo-TTY | `false` |
| `-i`, `--interactive` | Keep STDIN open | `false` |
| `--init` | Run init process | `false` |
| `--entrypoint` | Override default entrypoint | — |

```sh
# Run nginx in background with port forwarding
ocifbsd run -d --name web -p 8080:80 nginx:latest

# Run interactive shell
ocifbsd run -it --rm freebsd:latest /bin/sh

# Run with environment variables and volume
ocifbsd run -d \
  --name myapp \
  -e DATABASE_URL=postgres://db:5432/mydb \
  -v /data/myapp:/var/lib/myapp \
  -p 3000:3000 \
  myapp:latest

# Run with resource limits
ocifbsd run -d \
  --name limited \
  --memory 512M \
  --cpus 1.0 \
  --pids-limit 100 \
  myapp:latest

# Run with health check
ocifbsd run -d \
  --name healthy \
  --health-cmd "/usr/local/bin/healthcheck" \
  --health-interval 10s \
  --health-retries 3 \
  myapp:latest

# Run with VNET and static IP
ocifbsd run -d \
  --name nettest \
  --network bridge0 \
  --ip 192.168.100.50/24 \
  freebsd:latest

# Run one-off command that cleans up after itself
ocifbsd run --rm freebsd:latest pkg install -y curl
```

**`ocifbsd ps [OPTIONS]`**

List running containers.

| Flag | Description | Default |
|------|-------------|---------|
| `-a`, `--all` | Show all containers (including stopped) | `false` |
| `-f`, `--filter` | Filter output | — |
| `--format` | Custom output format | `table` |
| `-n`, `--last` | Show last N containers | `-1` (all) |
| `-l`, `--latest` | Show latest created container | `false` |
| `--noheading` | Omit column headers | `false` |
| `-q`, `--quiet` | Show only container IDs | `false` |
| `-s`, `--size` | Display total file sizes | `false` |

```sh
# List running containers
ocifbsd ps

# List all containers
ocifbsd ps -a

# Show only IDs
ocifbsd ps -q

# Filter by name
ocifbsd ps -a --filter "name=web*"

# Show latest container
ocifbsd ps -l
```

**Output format (`ocifbsd ps`):**

```
CONTAINER ID   IMAGE           COMMAND                  CREATED        STATUS         PORTS                    NAMES
a1b2c3d4e5f6   nginx:latest    "nginx -g daemon off"    2 hours ago    Up 2 hours     0.0.0.0:8080->80/tcp     web
b2c3d4e5f6a7   myapp:v1.0      "/usr/local/bin/myapp"   3 days ago     Up 3 days      0.0.0.0:3000->3000/tcp   myapp
```

**`ocifbsd stop [OPTIONS] CONTAINER [CONTAINER...]`**

Stop one or more running containers.

| Flag | Description | Default |
|------|-------------|---------|
| `-t`, `--time` | Seconds to wait before killing | `10` |
| `-f`, `--force` | Force kill immediately | `false` |

```sh
# Stop container gracefully
ocifbsd stop web

# Stop with custom timeout
ocifbsd stop -t 30 myapp

# Force stop
ocifbsd stop -f web

# Stop multiple containers
ocifbsd stop web myapp db
```

**`ocifbsd start [OPTIONS] CONTAINER [CONTAINER...]`**

Start one or more stopped containers.

| Flag | Description | Default |
|------|-------------|---------|
| `-a`, `--attach` | Attach STDOUT/STDERR | `false` |
| `-i`, `--interactive` | Attach STDIN | `false` |

```sh
# Start stopped container
ocifbsd start web

# Start and attach
ocifbsd start -a web
```

**`ocifbsd restart [OPTIONS] CONTAINER [CONTAINER...]`**

Restart one or more containers.

| Flag | Description | Default |
|------|-------------|---------|
| `-t`, `--time` | Seconds to wait before killing | `10` |

```sh
ocifbsd restart web
ocifbsd restart -t 5 myapp
```

**`ocifbsd rm [OPTIONS] CONTAINER [CONTAINER...]`**

Remove one or more containers.

| Flag | Description | Default |
|------|-------------|---------|
| `-f`, `--force` | Force removal of running container | `false` |
| `-v`, `--volumes` | Remove associated volumes | `false` |
| `-a`, `--all` | Remove all stopped containers | `false` |

```sh
# Remove stopped container
ocifbsd rm web

# Force remove running container
ocifbsd rm -f web

# Remove all stopped containers
ocifbsd rm -a
```

**`ocifbsd exec [OPTIONS] CONTAINER COMMAND [ARG...]`**

Execute a command in a running container.

| Flag | Description | Default |
|------|-------------|---------|
| `-d`, `--detach` | Run in background | `false` |
| `-e`, `--env` | Set environment variable | — |
| `-i`, `--interactive` | Keep STDIN open | `false` |
| `-t`, `--tty` | Allocate pseudo-TTY | `false` |
| `-u`, `--user` | User to run as | — |
| `-w`, `--workdir` | Working directory | — |
| `--privileged` | Run with extended privileges | `false` |

```sh
# Run shell in container
ocifbsd exec -it web /bin/sh

# Run command as specific user
ocifbsd exec -u www web ls -la /var/www

# Run background command
ocifbsd exec -d web /usr/local/bin/backup.sh

# Run with environment variable
ocifbsd exec -e DEBUG=1 web /usr/local/bin/myapp
```

**`ocifbsd logs [OPTIONS] CONTAINER`**

Fetch logs of a container.

| Flag | Description | Default |
|------|-------------|---------|
| `-f`, `--follow` | Follow log output | `false` |
| `--since` | Show logs since timestamp | — |
| `--tail` | Number of lines to show | `all` |
| `-t`, `--timestamps` | Show timestamps | `false` |
| `--until` | Show logs before timestamp | — |
| `--details` | Show extra attributes | `false` |

```sh
# Show all logs
ocifbsd logs web

# Follow logs
ocifbsd logs -f web

# Show last 100 lines
ocifbsd logs --tail 100 web

# Show logs since specific time
ocifbsd logs --since 2026-04-01T00:00:00 web
```

**`ocifbsd inspect [OPTIONS] CONTAINER`**

Display detailed information about a container.

| Flag | Description | Default |
|------|-------------|---------|
| `-f`, `--format` | Format output using Go template | — |
| `-s`, `--size` | Display total file sizes | `false` |
| `--type` | Return JSON for specified type | — |

```sh
# Inspect container
ocifbsd inspect web

# Get specific field
ocifbsd inspect -f '{{.NetworkSettings.IPAddress}}' web

# Get state
ocifbsd inspect -f '{{.State.Status}}' web
```

**`ocifbsd top [OPTIONS] CONTAINER [PS_OPTIONS]`**

Display running processes of a container.

```sh
# Show processes
ocifbsd top web

# Show with custom ps options
ocifbsd top web -o pid,command
```

**`ocifbsd stats [OPTIONS] [CONTAINER...]`**

Display live stream of container resource usage.

| Flag | Description | Default |
|------|-------------|---------|
| `-a`, `--all` | Show all containers | `false` |
| `--format` | Custom output format | `table` |
| `--noheading` | Omit column headers | `false` |
| `--no-stream` | Disable streaming | `false` |

```sh
# Show stats for all running containers
ocifbsd stats

# Show stats for specific container
ocifbsd stats web

# One-shot output
ocifbsd stats --no-stream web
```

**Output format (`ocifbsd stats`):**

```
CONTAINER ID   NAME   CPU %   MEM USAGE / LIMIT   MEM %   NET I/O         BLOCK I/O   PIDS
a1b2c3d4e5f6   web    0.05%   15.2MB / 512MB      2.97%   1.2MB / 850KB   0B / 0B     5
```

**`ocifbsd cp [OPTIONS] CONTAINER:SRC_PATH DEST_PATH`  
`ocifbsd cp [OPTIONS] SRC_PATH CONTAINER:DEST_PATH`**

Copy files/folders between a container and the local filesystem.

| Flag | Description | Default |
|------|-------------|---------|
| `-a`, `--archive` | Archive mode (copy all attributes) | `false` |
| `-L`, `--follow-link` | Follow symbolic links | `false` |
| `-q`, `--quiet` | Suppress output | `false` |

```sh
# Copy from container to host
ocifbsd cp web:/usr/local/www/nginx/dist/index.html ./index.html

# Copy from host to container
ocifbsd cp ./config.ini myapp:/usr/local/etc/myapp/

# Copy directory
ocifbsd cp -a ./static web:/usr/local/www/nginx/dist/
```

**`ocifbsd diff CONTAINER`**

Inspect changes to files or directories on a container's filesystem.

```sh
ocifbsd diff web
```

**Output format (`ocifbsd diff`):**

```
C /etc/nginx/nginx.conf
A /var/log/nginx/access.log
D /tmp/oldfile
```

**`ocifbsd commit [OPTIONS] CONTAINER [IMAGE[:TAG]]`**

Create a new image from a container's changes.

| Flag | Description | Default |
|------|-------------|---------|
| `-a`, `--author` | Author | — |
| `-m`, `--message` | Commit message | — |
| `-p`, `--pause` | Pause container during commit | `true` |
| `--change` | Apply Dockerfile instruction | — |

```sh
# Commit container changes
ocifbsd commit web mynginx:custom

# Commit with message
ocifbsd commit -m "Added custom config" web mynginx:v2
```

**`ocifbsd pause CONTAINER`**

Pause all processes within a container.

```sh
ocifbsd pause web
```

**`ocifbsd unpause CONTAINER`**

Unpause all processes within a container.

```sh
ocifbsd unpause web
```

**`ocifbsd wait [OPTIONS] CONTAINER [CONTAINER...]`**

Block until container stops, then print exit code.

```sh
ocifbsd wait web
```

**`ocifbsd rename CONTAINER NEW_NAME`**

Rename a container.

```sh
ocifbsd rename web frontend
```

**`ocifbsd update [OPTIONS] CONTAINER`**

Update configuration of a container.

| Flag | Description |
|------|-------------|
| `--memory` | Memory limit |
| `--memory-swap` | Swap limit |
| `--cpus` | CPU limit |
| `--pids-limit` | PID limit |
| `--restart` | Restart policy |

```sh
ocifbsd update --memory 1G web
```

#### 6.4.4 Low-Level OCI Runtime Commands

These commands map directly to the OCI runtime specification and are used for advanced use cases and integration with orchestrators.

**`ocifbsd create [OPTIONS] CONTAINER`**

Create a new container (does not start it).

| Flag | Description | Default |
|------|-------------|---------|
| `--bundle` | Path to OCI bundle | `.` |
| `--console-socket` | Path to AF_UNIX socket | — |
| `--pid-file` | Path to write container PID | — |
| `--no-new-keyring` | Do not create new session keyring | `false` |

```sh
ocifbsd create --bundle /var/lib/ocifbsd/bundles/web web
```

**`ocifbsd start CONTAINER`**

Start an existing container.

```sh
ocifbsd start web
```

**`ocifbsd kill [OPTIONS] CONTAINER SIGNAL`**

Send a signal to a container.

| Flag | Description | Default |
|------|-------------|---------|
| `-a`, `--all` | Send to all processes | `false` |

```sh
ocifbsd kill web TERM
ocifbsd kill -s HUP web
```

**`ocifbsd delete [OPTIONS] CONTAINER`**

Delete a container.

| Flag | Description | Default |
|------|-------------|---------|
| `-f`, `--force` | Force deletion | `false` |

```sh
ocifbsd delete web
```

**`ocifbsd state CONTAINER`**

Output the state of a container in OCI state JSON format.

```sh
ocifbsd state web
```

**`ocifbsd checkpoint [OPTIONS] CONTAINER`**

Checkpoint a running container (future enhancement).

**`ocifbsd restore [OPTIONS] CONTAINER`**

Restore a container from a checkpoint (future enhancement).

#### 6.4.5 Network Commands

**`ocifbsd network create [OPTIONS] NETWORK`**

Create a new network.

| Flag | Description | Default |
|------|-------------|---------|
| `-d`, `--driver` | Network driver | `bridge` |
| `--subnet` | Subnet in CIDR format | — |
| `--gateway` | Gateway for subnet | — |
| `--ip-range` | Allocate container IPs from range | — |
| `--label` | Set metadata | — |
| `--internal` | Restrict external access | `false` |
| `--ipv6` | Enable IPv6 | `false` |

```sh
# Create bridge network
ocifbsd network create --subnet 192.168.100.0/24 --gateway 192.168.100.1 mynet

# Create internal network
ocifbsd network create --internal backend
```

**`ocifbsd network ls [OPTIONS]`**

List networks.

| Flag | Description | Default |
|------|-------------|---------|
| `-f`, `--filter` | Filter output | — |
| `--format` | Custom output format | `table` |
| `--noheading` | Omit headers | `false` |
| `-q`, `--quiet` | Show only names | `false` |

```sh
ocifbsd network ls
```

**`ocifbsd network rm NETWORK [NETWORK...]`**

Remove one or more networks.

```sh
ocifbsd network rm mynet
```

**`ocifbsd network inspect [OPTIONS] NETWORK [NETWORK...]`**

Display detailed information about networks.

| Flag | Description | Default |
|------|-------------|---------|
| `-f`, `--format` | Format output | — |
| `--verbose` | Verbose output | `false` |

```sh
ocifbsd network inspect mynet
```

**`ocifbsd network connect [OPTIONS] NETWORK CONTAINER`**

Connect a container to a network.

| Flag | Description |
|------|-------------|
| `--ip` | IPv4 address |
| `--ip6` | IPv6 address |
| `--alias` | Add network-scoped alias |

```sh
ocifbsd network connect --ip 192.168.100.50 mynet web
```

**`ocifbsd network disconnect [OPTIONS] NETWORK CONTAINER`**

Disconnect a container from a network.

| Flag | Description | Default |
|------|-------------|---------|
| `-f`, `--force` | Force disconnect | `false` |

```sh
ocifbsd network disconnect mynet web
```

#### 6.4.6 Volume Commands

**`ocifbsd volume create [OPTIONS] [VOLUME]`**

Create a new volume.

| Flag | Description | Default |
|------|-------------|---------|
| `-d`, `--driver` | Volume driver | `zfs` |
| `--label` | Set metadata | — |
| `-o`, `--opt` | Driver-specific options | — |

```sh
# Create ZFS-backed volume
ocifbsd volume create mydata

# Create with specific ZFS dataset
ocifbsd volume create -o dataset=zroot/volumes/mydata mydata
```

**`ocifbsd volume ls [OPTIONS]`**

List volumes.

| Flag | Description | Default |
|------|-------------|---------|
| `-f`, `--filter` | Filter output | — |
| `--format` | Custom output format | `table` |
| `-q`, `--quiet` | Show only names | `false` |

```sh
ocifbsd volume ls
```

**`ocifbsd volume rm [OPTIONS] VOLUME [VOLUME...]`**

Remove one or more volumes.

| Flag | Description | Default |
|------|-------------|---------|
| `-f`, `--force` | Force removal | `false` |

```sh
ocifbsd volume rm mydata
```

**`ocifbsd volume inspect [OPTIONS] VOLUME [VOLUME...]`**

Display detailed information about volumes.

```sh
ocifbsd volume inspect mydata
```

**`ocifbsd volume prune [OPTIONS]`**

Remove all unused volumes.

| Flag | Description | Default |
|------|-------------|---------|
| `-f`, `--force` | Do not prompt | `false` |

```sh
ocifbsd volume prune
```

#### 6.4.7 System Commands

**`ocifbsd system df [OPTIONS]`**

Show disk usage.

| Flag | Description | Default |
|------|-------------|---------|
| `-v`, `--verbose` | Show detailed space usage | `false` |
| `--format` | Custom output format | `table` |

```sh
ocifbsd system df
```

**`ocifbsd system info`**

Display system-wide information.

```sh
ocifbsd system info
```

**`ocifbsd system prune [OPTIONS]`**

Remove unused data.

| Flag | Description | Default |
|------|-------------|---------|
| `-a`, `--all` | Remove all unused data | `false` |
| `-f`, `--force` | Do not prompt | `false` |
| `--volumes` | Prune volumes | `false` |

```sh
ocifbsd system prune
ocifbsd system prune -a
```

**`ocifbsd system events [OPTIONS]`**

Get real-time events from the server.

| Flag | Description | Default |
|------|-------------|---------|
| `-f`, `--filter` | Filter events | — |
| `--since` | Show events created after timestamp | — |
| `--until` | Show events created before timestamp | — |
| `--format` | Format output | `json` |

```sh
ocifbsd system events
```

**`ocifbsd version`**

Show version information.

```sh
ocifbsd version
```

---

### 6.5 Docker/Podman Compatibility Layer

To ease migration for users coming from Linux container ecosystems, `ocifbsd` provides a compatibility wrapper that translates familiar `docker` and `podman` commands into native `ocifbsd` operations.

#### 6.5.1 Compatibility Wrapper: `docker` and `podman` Aliases

**Installation:**

```sh
# Create symlinks (optional, during package install)
ln -s /usr/sbin/ocifbsd /usr/local/bin/docker
ln -s /usr/sbin/ocifbsd /usr/local/bin/podman
```

When invoked as `docker` or `podman`, `ocifbsd` detects the invocation name and translates commands:

| Docker/Podman Command | ocifbsd Equivalent | Notes |
|----------------------|-------------------|-------|
| `docker run` | `ocifbsd run` | Full compatibility |
| `docker ps` | `ocifbsd ps` | Full compatibility |
| `docker images` | `ocifbsd images` | Full compatibility |
| `docker pull` | `ocifbsd pull` | Full compatibility |
| `docker push` | `ocifbsd push` | Full compatibility |
| `docker build` | `ocifbsd build` | See Containerfile support |
| `docker rm` | `ocifbsd rm` | Full compatibility |
| `docker rmi` | `ocifbsd rmi` | Full compatibility |
| `docker exec` | `ocifbsd exec` | Full compatibility |
| `docker logs` | `ocifbsd logs` | Full compatibility |
| `docker stop` | `ocifbsd stop` | Full compatibility |
| `docker start` | `ocifbsd start` | Full compatibility |
| `docker restart` | `ocifbsd restart` | Full compatibility |
| `docker inspect` | `ocifbsd inspect` | Full compatibility |
| `docker cp` | `ocifbsd cp` | Full compatibility |
| `docker diff` | `ocifbsd diff` | Full compatibility |
| `docker commit` | `ocifbsd commit` | Full compatibility |
| `docker pause` | `ocifbsd pause` | Full compatibility |
| `docker unpause` | `ocifbsd unpause` | Full compatibility |
| `docker wait` | `ocifbsd wait` | Full compatibility |
| `docker rename` | `ocifbsd rename` | Full compatibility |
| `docker update` | `ocifbsd update` | Full compatibility |
| `docker top` | `ocifbsd top` | Full compatibility |
| `docker stats` | `ocifbsd stats` | Full compatibility |
| `docker save` | `ocifbsd save` | Full compatibility |
| `docker load` | `ocifbsd load` | Full compatibility |
| `docker tag` | `ocifbsd tag` | Full compatibility |
| `docker network *` | `ocifbsd network *` | Full compatibility |
| `docker volume *` | `ocifbsd volume *` | Full compatibility |
| `docker system *` | `ocifbsd system *` | Full compatibility |
| `docker version` | `ocifbsd version` | Full compatibility |
| `docker info` | `ocifbsd system info` | Mapped |
| `docker login` | `ocifbsd login` | Registry authentication |
| `docker logout` | `ocifbsd logout` | Registry authentication |
| `docker buildx` | `ocifbsd build` | Simplified mapping |
| `docker compose` | `ocifbsd compose` | See Section 6.7 |
| `docker swarm` | N/A | Not supported (use Kubernetes CRI) |
| `docker context` | N/A | Not applicable |
| `docker secret` | N/A | Use jail secrets or files |
| `docker config` | N/A | Use configuration files |
| `docker service` | N/A | Use Kubernetes CRI |
| `docker stack` | N/A | Use Kubernetes CRI |

#### 6.5.2 Compatibility Configuration

Users can configure compatibility behavior in `/etc/ocifbsd/ocifbsd.conf`:

```sh
# Compatibility settings
compatibility_mode="docker"     # "docker", "podman", or "native"
warn_on_compatibility="true"    # Warn when using compatibility aliases
auto_alias="false"              # Auto-create docker/podman symlinks
```

#### 6.5.3 Unsupported Docker Features (Documented)

| Feature | Status | FreeBSD Alternative |
|---------|--------|-------------------|
| Linux cgroups | Not applicable | RCTL |
| Linux namespaces | Not applicable | Jail |
| seccomp | Not applicable | MAC framework |
| AppArmor | Not applicable | MAC framework |
| SELinux | Not applicable | MAC framework |
| overlayfs | Not applicable | ZFS clones |
| user namespaces (rootless) | Limited | Capsicum, unprivileged jail research |
| systemd integration | Not applicable | rc.d scripts |
| OCI hooks (Linux-specific) | Filtered | FreeBSD-specific hooks |

---

### 6.6 User Workflow Examples

This section demonstrates day-to-day container operations from a user perspective, showing both the `ocifbsd` native commands and their Docker/Podman equivalents.

#### 6.6.1 Getting Started: First Container

```sh
# Pull a FreeBSD base image
ocifbsd pull freebsd:latest

# Run an interactive shell to explore
ocifbsd run -it --rm freebsd:latest /bin/sh

# Inside container:
#   uname -a
#   pkg install -y curl
#   exit

# Run a web server in the background
ocifbsd run -d --name web -p 8080:80 nginx:latest

# Check it's running
ocifbsd ps

# Test the web server
curl http://localhost:8080

# View logs
ocifbsd logs web

# Stop and remove
ocifbsd stop web
ocifbsd rm web
```

**Docker equivalent:**
```sh
docker pull freebsd:latest
docker run -it --rm freebsd:latest /bin/sh
docker run -d --name web -p 8080:80 nginx:latest
docker ps
curl http://localhost:8080
docker logs web
docker stop web && docker rm web
```

#### 6.6.2 Development Workflow

```sh
# Build application image
ocifbsd build -t myapp:dev .

# Run with live code mounting
ocifbsd run -d \
  --name myapp-dev \
  -v $(pwd)/src:/app/src \
  -v $(pwd)/config:/app/config \
  -e DEBUG=1 \
  -e DATABASE_URL=postgres://dev:5432/myapp \
  -p 3000:3000 \
  myapp:dev

# Watch logs during development
ocifbsd logs -f myapp-dev

# Execute commands in running container
ocifbsd exec -it myapp-dev /bin/sh
ocifbsd exec myapp-dev pkg install -y gdb

# Run tests
ocifbsd exec myapp-dev make test

# Restart after code changes
ocifbsd restart myapp-dev

# Clean up when done
ocifbsd rm -f myapp-dev
```

#### 6.6.3 Production Deployment Workflow

```sh
# Build production image
ocifbsd build -t myapp:v1.0.0 -f Containerfile.prod .

# Tag for registry
ocifbsd tag myapp:v1.0.0 registry.example.com/myapp:v1.0.0

# Push to registry
ocifbsd push registry.example.com/myapp:v1.0.0

# Deploy on production host
ocifbsd run -d \
  --name myapp-prod \
  --restart always \
  --memory 2G \
  --cpus 2.0 \
  --pids-limit 500 \
  -e DATABASE_URL="${DB_URL}" \
  -e SECRET_KEY="${SECRET_KEY}" \
  -v myapp-data:/var/lib/myapp \
  --network prod-net \
  --ip 192.168.10.50 \
  --health-cmd "/usr/local/bin/healthcheck" \
  --health-interval 30s \
  --health-retries 3 \
  --label env=production \
  --label version=1.0.0 \
  registry.example.com/myapp:v1.0.0

# Monitor
ocifbsd stats myapp-prod
ocifbsd top myapp-prod

# Scale by running multiple instances
ocifbsd run -d --name myapp-prod-2 ...
ocifbsd run -d --name myapp-prod-3 ...

# Rolling update: start new version, stop old
ocifbsd run -d --name myapp-prod-new registry.example.com/myapp:v1.0.1
ocifbsd stop myapp-prod
ocifbsd rm myapp-prod
ocifbsd rename myapp-prod-new myapp-prod
```

#### 6.6.4 Database and Service Stack

```sh
# Create network for services
ocifbsd network create --subnet 192.168.50.0/24 app-net

# Run PostgreSQL
ocifbsd run -d \
  --name postgres \
  --network app-net \
  --ip 192.168.50.10 \
  -e POSTGRES_DB=myapp \
  -e POSTGRES_USER=myuser \
  -e POSTGRES_PASSWORD=secret \
  -v postgres-data:/var/db/postgres \
  --memory 1G \
  postgres:14

# Run Redis
ocifbsd run -d \
  --name redis \
  --network app-net \
  --ip 192.168.50.11 \
  -v redis-data:/var/db/redis \
  --memory 512M \
  redis:latest

# Run application connected to both
ocifbsd run -d \
  --name myapp \
  --network app-net \
  --ip 192.168.50.20 \
  -e DATABASE_URL=postgres://myuser:secret@192.168.50.10:5432/myapp \
  -e REDIS_URL=redis://192.168.50.11:6379 \
  -p 8080:8080 \
  --memory 1G \
  myapp:latest

# View all running services
ocifbsd ps

# Check connectivity
ocifbsd exec myapp ping -c 3 192.168.50.10
ocifbsd exec myapp redis-cli -h 192.168.50.11 ping
```

#### 6.6.5 Backup and Maintenance

```sh
# Backup container data volume
ocifbsd run --rm \
  -v myapp-data:/data \
  -v /backup:/backup \
  freebsd:latest \
  tar czf /backup/myapp-data-$(date +%Y%m%d).tar.gz /data

# Export container filesystem
ocifbsd export web > web-backup.tar

# Create image from running container
ocifbsd commit -m "Before upgrade" web web:pre-upgrade

# Update base image and recreate
ocifbsd pull freebsd:latest
ocifbsd stop web
ocifbsd rm web
ocifbsd run -d --name web -p 8080:80 myapp:latest

# Prune unused resources
ocifbsd system prune -a
ocifbsd volume prune
```

#### 6.6.6 Troubleshooting Workflow

```sh
# Container won't start - check logs
ocifbsd logs --tail 50 myapp

# Inspect container configuration
ocifbsd inspect myapp

# Check resource usage
ocifbsd stats --no-stream myapp

# Enter container for debugging
ocifbsd exec -it myapp /bin/sh

# Check network connectivity
ocifbsd exec myapp netstat -rn
ocifbsd exec myapp sockstat -4

# Copy files for analysis
ocifbsd cp myapp:/var/log/myapp/error.log ./error.log

# Compare with working container
ocifbsd diff myapp

# Check jail parameters directly
jls -j myapp

# Check RCTL limits
rctl -j myapp

# Restart with debug output
ocifbsd stop myapp
ocifbsd run -d --name myapp-debug -e DEBUG=1 myapp:latest
ocifbsd logs -f myapp-debug
```

---

### 6.7 Multi-Container Orchestration: `ocifbsd compose`

While full Kubernetes CRI integration is a future enhancement (Section 12), `ocifbsd` provides a Docker Compose-compatible orchestration command for local multi-container development and small deployments.

#### 6.7.1 Compose File Format

`ocifbsd compose` reads `oci-compose.yml` or `oci-compose.yaml` files:

```yaml
# oci-compose.yml
version: "3.9"

services:
  web:
    image: nginx:latest
    container_name: web
    ports:
      - "8080:80"
    volumes:
      - ./html:/usr/local/www/nginx:ro
      - nginx-cache:/var/cache/nginx
    networks:
      - frontend
    depends_on:
      - api
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost/"]
      interval: 30s
      timeout: 10s
      retries: 3
    labels:
      - "env=production"

  api:
    build:
      context: ./api
      dockerfile: Containerfile
    container_name: api
    ports:
      - "3000:3000"
    environment:
      - DATABASE_URL=postgres://db:5432/myapp
      - REDIS_URL=redis://cache:6379
    volumes:
      - ./api/src:/app/src
    networks:
      - frontend
      - backend
    depends_on:
      - db
      - cache
    restart: unless-stopped
    deploy:
      resources:
        limits:
          memory: 512M
          cpus: '1.0'

  db:
    image: postgres:14
    container_name: postgres
    environment:
      POSTGRES_DB: myapp
      POSTGRES_USER: myuser
      POSTGRES_PASSWORD: secret
    volumes:
      - postgres-data:/var/db/postgres
    networks:
      - backend
    restart: always
    deploy:
      resources:
        limits:
          memory: 1G

  cache:
    image: redis:latest
    container_name: redis
    volumes:
      - redis-data:/var/db/redis
    networks:
      - backend
    restart: always

networks:
  frontend:
    driver: bridge
    ipam:
      config:
        - subnet: 192.168.100.0/24
  backend:
    driver: bridge
    internal: true
    ipam:
      config:
        - subnet: 192.168.200.0/24

volumes:
  nginx-cache:
    driver: zfs
  postgres-data:
    driver: zfs
    driver_opts:
      dataset: zroot/postgres
  redis-data:
    driver: zfs
```

#### 6.7.2 Compose Commands

| Command | Description |
|---------|-------------|
| `ocifbsd compose up` | Create and start all services |
| `ocifbsd compose up -d` | Start in background |
| `ocifbsd compose down` | Stop and remove all services |
| `ocifbsd compose down -v` | Also remove volumes |
| `ocifbsd compose ps` | List running services |
| `ocifbsd compose logs` | View logs from all services |
| `ocifbsd compose logs -f` | Follow logs |
| `ocifbsd compose build` | Build all services |
| `ocifbsd compose pull` | Pull all images |
| `ocifbsd compose restart` | Restart all services |
| `ocifbsd compose stop` | Stop all services |
| `ocifbsd compose start` | Start stopped services |
| `ocifbsd compose rm` | Remove stopped containers |
| `ocifbsd compose exec SERVICE COMMAND` | Execute command in service |
| `ocifbsd compose run SERVICE COMMAND` | Run one-off command |
| `ocifbsd compose config` | Validate and view config |
| `ocifbsd compose top` | Display running processes |
| `ocifbsd compose events` | Receive real-time events |
| `ocifbsd compose pause` | Pause all services |
| `ocifbsd compose unpause` | Unpause all services |

```sh
# Start all services
ocifbsd compose up -d

# View logs
ocifbsd compose logs -f

# Scale a service
ocifbsd compose up -d --scale api=3

# Rebuild after code changes
ocifbsd compose build api
ocifbsd compose up -d api

# Run one-off command
ocifbsd compose run --rm api make migrate

# Clean up everything
ocifbsd compose down -v
```

#### 6.7.3 Compose FreeBSD Extensions

FreeBSD-specific extensions to the compose format:

```yaml
services:
  myapp:
    image: myapp:latest
    freebsd:
      vnet: true
      mac_label: "biba/high"
      rctl:
        memoryuse: "512M"
        pcpu: "50"
      devfs_ruleset: 4
      allow_chflags: true
      allow_mount: false
      allow_raw_sockets: false
```

---

### 6.8 Containerfile / OCIfile Format

`ocifbsd build` supports a Dockerfile-compatible build specification with FreeBSD-specific extensions.

#### 6.8.1 Supported Instructions

| Instruction | Description | Example |
|-------------|-------------|---------|
| `FROM` | Base image | `FROM freebsd:14.0` |
| `RUN` | Execute command | `RUN pkg install -y nginx` |
| `CMD` | Default command | `CMD ["nginx", "-g", "daemon off;"]` |
| `ENTRYPOINT` | Entry point | `ENTRYPOINT ["/usr/local/bin/myapp"]` |
| `COPY` | Copy files | `COPY . /app` |
| `ADD` | Copy with extraction | `ADD https://example.com/file.tar.gz /tmp/` |
| `ENV` | Environment variable | `ENV DEBUG=1` |
| `ARG` | Build argument | `ARG VERSION=latest` |
| `LABEL` | Metadata | `LABEL maintainer="user@example.com"` |
| `EXPOSE` | Expose port | `EXPOSE 8080/tcp` |
| `VOLUME` | Create mount point | `VOLUME /var/lib/myapp` |
| `WORKDIR` | Working directory | `WORKDIR /app` |
| `USER` | User to run as | `USER www` |
| `SHELL` | Default shell | `SHELL ["/bin/sh", "-c"]` |
| `HEALTHCHECK` | Health check | `HEALTHCHECK CMD /usr/local/bin/check` |
| `STOPSIGNAL` | Stop signal | `STOPSIGNAL SIGTERM` |

#### 6.8.2 FreeBSD-Specific Extensions

```dockerfile
# FreeBSD-specific directives
FREEBSD-VNET true
FREEBSD-MAC biba/high
FREEBSD-RCTL memoryuse=512M,pcpu=50
FREEBSD-DEVFS ruleset=4
FREEBSD-ALLOW chflags,mount
```

Or using `LABEL` for compatibility:

```dockerfile
LABEL freebsd.vnet="true"
LABEL freebsd.mac="biba/high"
LABEL freebsd.rctl="memoryuse=512M,pcpu=50"
```

#### 6.8.3 Example Containerfile

```dockerfile
# Containerfile for a FreeBSD web application
FROM freebsd:14.0

LABEL maintainer="dev@example.com"
LABEL freebsd.vnet="true"

# Install dependencies
RUN pkg install -y \
    nginx \
    python39 \
    py39-pip \
    && pkg clean -y

# Create application user
RUN pw useradd -n myapp -d /nonexistent -s /usr/sbin/nologin

# Set working directory
WORKDIR /app

# Copy application
COPY requirements.txt .
RUN pip install -r requirements.txt

COPY . .

# Set permissions
RUN chown -R myapp:myapp /app

# Expose port
EXPOSE 8080

# Health check
HEALTHCHECK --interval=30s --timeout=10s --retries=3 \
    CMD curl -f http://localhost:8080/health || exit 1

# Switch to non-root user
USER myapp

# Default command
CMD ["python", "-m", "myapp"]
```

#### 6.8.4 Build Command

```sh
# Build from Containerfile
ocifbsd build -t myapp:latest .

# Build with specific file
ocifbsd build -t myapp:latest -f Containerfile.prod .

# Build with build arguments
ocifbsd build -t myapp:latest --build-arg VERSION=1.0.0 .

# Build without cache
ocifbsd build -t myapp:latest --no-cache .

# Build and push
ocifbsd build -t registry.example.com/myapp:latest --push .
```

---

### 6.9 Output Formats and Filtering

#### 6.9.1 Standard Output Formats

All list commands support multiple output formats:

**Table (default):**
```sh
ocifbsd ps
```

**JSON:**
```sh
ocifbsd ps --format json
ocifbsd images --format json
ocifbsd network ls --format json
```

**Custom Go template:**
```sh
ocifbsd ps --format "{{.Names}}\t{{.Status}}"
ocifbsd inspect -f '{{.NetworkSettings.IPAddress}}' web
```

**Quiet (IDs/names only):**
```sh
ocifbsd ps -q
ocifbsd images -q
```

#### 6.9.2 Filtering

Filter syntax follows Docker/Podman conventions:

```sh
# Filter containers
ocifbsd ps -a --filter "name=web*"
ocifbsd ps -a --filter "status=running"
ocifbsd ps -a --filter "label=env=production"
ocifbsd ps -a --filter "ancestor=freebsd:latest"

# Filter images
ocifbsd images --filter "dangling=true"
ocifbsd images --filter "label=version=1.0"

# Filter volumes
ocifbsd volume ls --filter "dangling=true"

# Filter networks
ocifbsd network ls --filter "driver=bridge"
```

#### 6.9.3 libxo Integration

Following FreeBSD conventions (as used by `jls`, `ifconfig`, `vmstat`), `ocifbsd` integrates with `libxo` for structured output:

```sh
# XML output
ocifbsd ps --libxo xml

# JSON output (via libxo)
ocifbsd ps --libxo json

# HTML output
ocifbsd ps --libxo html
```

---

### 6.10 Integration with Existing FreeBSD Tools

`ocifbsd` is designed to complement, not replace, existing FreeBSD userland tools. Users can drop down to native tools at any time.

#### 6.10.1 Jail Integration

```sh
# ocifbsd creates jails with descriptive names
ocifbsd run -d --name web nginx:latest

# Use jls to see all jails (including ocifbsd-managed)
jls
jls -v

# Use jexec to enter any jail
jexec web /bin/sh

# Use jail command for advanced operations
jail -r web  # Remove jail (use ocifbsd rm instead for cleanup)
```

#### 6.10.2 ZFS Integration

```sh
# ocifbsd stores images and volumes as ZFS datasets
ocifbsd images

# Use zfs command to inspect
zfs list -r zroot/ocifbsd
zfs list -r zroot/ocifbsd/images
zfs list -r zroot/ocifbsd/volumes
zfs list -r zroot/ocifbsd/containers

# Manual snapshot (ocifbsd handles this automatically)
zfs snapshot zroot/ocifbsd/containers/web@manual-backup

# Check dataset properties
zfs get all zroot/ocifbsd/containers/web
```

#### 6.10.3 Network Integration

```sh
# ocifbsd creates VNET interfaces and bridges
ocifbsd network ls

# Use ifconfig to inspect
ifconfig
ifconfig bridge0
ifconfig vnet0

# Use pfctl for firewall rules
pfctl -sr | grep ocifbsd
pfctl -sn | grep ocifbsd

# Use netstat for connections
netstat -rn
sockstat -4
```

#### 6.10.4 Resource Control Integration

```sh
# ocifbsd applies RCTL rules
ocifbsd run --memory 512M --cpus 1.0 myapp

# Use rctl to inspect
rctl -j web
rctl -u jail:web

# Use rctl to modify (advanced)
rctl -a jail:web:memoryuse:deny=1G
```

#### 6.10.5 Service Integration

```sh
# Enable container auto-start
sysrc ocifbsd_enable=YES
sysrc ocifbsd_containers="web api db"

# Use service command
service ocifbsd start
service ocifbsd stop
service ocifbsd restart
service ocifbsd status

# View startup log
cat /var/log/ocifbsd.log
```

#### 6.10.6 Package Management Integration

```sh
# Install ocifbsd via pkg
pkg install ocifbsd

# Or build from ports
cd /usr/ports/sysutils/ocifbsd && make install

# Update
pkg upgrade ocifbsd
```

---

### 6.11 Shell Completion

`ocifbsd` provides shell completion scripts for `sh`, `csh`, and `bash`:

```sh
# Install bash completion
ocifbsd completion bash > /usr/local/etc/bash_completion.d/ocifbsd

# Install zsh completion
ocifbsd completion zsh > /usr/local/share/zsh/site-functions/_ocifbsd

# Install fish completion
ocifbsd completion fish > ~/.config/fish/completions/ocifbsd.fish
```

---

### 6.12 Migration Guide: Docker/Podman to ocifbsd

#### 6.12.1 Command Mapping Quick Reference

| Task | Docker/Podman | ocifbsd |
|------|--------------|---------|
| Pull image | `docker pull img` | `ocifbsd pull img` |
| Run container | `docker run -d -p 80:80 nginx` | `ocifbsd run -d -p 80:80 nginx` |
| List containers | `docker ps` | `ocifbsd ps` |
| Stop container | `docker stop web` | `ocifbsd stop web` |
| Remove container | `docker rm web` | `ocifbsd rm web` |
| Build image | `docker build -t myapp .` | `ocifbsd build -t myapp .` |
| View logs | `docker logs web` | `ocifbsd logs web` |
| Execute command | `docker exec -it web sh` | `ocifbsd exec -it web sh` |
| Copy files | `docker cp web:/file .` | `ocifbsd cp web:/file .` |
| Compose up | `docker compose up -d` | `ocifbsd compose up -d` |

#### 6.12.2 Dockerfile to Containerfile Migration

Most Dockerfiles work unchanged as Containerfiles. Key differences:

1. **Base images:** Use `FROM freebsd:VERSION` instead of `FROM ubuntu:VERSION`
2. **Package manager:** Use `pkg` instead of `apt`
3. **User management:** Use `pw` instead of `useradd`
4. **Service paths:** Use `/usr/local/etc/` instead of `/etc/`
5. **Log paths:** Use `/var/log/` (standard FreeBSD)

#### 6.12.3 docker-compose.yml to oci-compose.yml Migration

Most `docker-compose.yml` files work with minimal changes:

1. Change `version` to `"3.9"` or higher
2. Add `freebsd:` section for FreeBSD-specific features
3. Use FreeBSD-appropriate image names
4. Adjust volume paths for FreeBSD conventions

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
