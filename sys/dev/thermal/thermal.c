#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/sysctl.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>

#define THERMAL_MSR_THERM_STATUS 0x19C
#define THERMAL_MSR_THERM_INTERRUPT 0x19B
#define THERMAL_MSR_PACKAGE_THERM_STATUS 0x1B1
#define THERMAL_MAX_TEMP 100

struct thermal_softc {
    device_t dev;
    struct cdev *cdev;
    struct mtx mtx;
    int current_temp;
    int critical_temp;
    int throttle_count;
    struct sysctl_ctx_list sysctl_ctx;
    struct sysctl_oid *sysctl_tree;
};

static struct cdevsw thermal_cdevsw = {
    .d_version = D_VERSION,
    .d_open = thermal_open,
    .d_close = thermal_close,
    .d_read = thermal_read,
    .d_name = "thermal",
};

static int
thermal_read_msr(uint32_t msr, uint64_t *val)
{
    if (msr == THERMAL_MSR_THERM_STATUS || 
        msr == THERMAL_MSR_THERM_INTERRUPT ||
        msr == THERMAL_MSR_PACKAGE_THERM_STATUS) {
        *val = rdmsr(msr);
        return 0;
    }
    return EINVAL;
}

static int
thermal_get_temperature(struct thermal_softc *sc)
{
    uint64_t msr_val;
    int temp;
    
    if (thermal_read_msr(THERMAL_MSR_THERM_STATUS, &msr_val) != 0)
        return -1;
        
    if (!(msr_val & 0x80000000))
        return -1;
        
    temp = (msr_val >> 16) & 0x7F;
    return THERMAL_MAX_TEMP - temp;
}

static int
thermal_check_throttle(struct thermal_softc *sc)
{
    uint64_t msr_val;
    
    if (thermal_read_msr(THERMAL_MSR_THERM_STATUS, &msr_val) != 0)
        return 0;
        
    return (msr_val & 0x10) ? 1 : 0;
}

static int
thermal_temp_sysctl(SYSCTL_HANDLER_ARGS)
{
    struct thermal_softc *sc = arg1;
    int temp;
    
    mtx_lock(&sc->mtx);
    temp = thermal_get_temperature(sc);
    if (temp >= 0)
        sc->current_temp = temp;
    mtx_unlock(&sc->mtx);
    
    return sysctl_handle_int(oidp, &sc->current_temp, 0, req);
}

static int
thermal_throttle_sysctl(SYSCTL_HANDLER_ARGS)
{
    struct thermal_softc *sc = arg1;
    
    mtx_lock(&sc->mtx);
    if (thermal_check_throttle(sc))
        sc->throttle_count++;
    mtx_unlock(&sc->mtx);
    
    return sysctl_handle_int(oidp, &sc->throttle_count, 0, req);
}

static int
thermal_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
    struct thermal_softc *sc = dev->si_drv1;
    
    if (!(cpu_feature & CPUID_ACPI))
        return ENODEV;
        
    mtx_lock(&sc->mtx);
    sc->current_temp = thermal_get_temperature(sc);
    mtx_unlock(&sc->mtx);
    
    return 0;
}

static int
thermal_close(struct cdev *dev, int fflag, int devtype, struct thread *td)
{
    return 0;
}

static int
thermal_read(struct cdev *dev, struct uio *uio, int ioflag)
{
    struct thermal_softc *sc = dev->si_drv1;
    char buf[64];
    int len, temp;
    
    mtx_lock(&sc->mtx);
    temp = thermal_get_temperature(sc);
    if (temp >= 0)
        sc->current_temp = temp;
    mtx_unlock(&sc->mtx);
    
    len = snprintf(buf, sizeof(buf), "temp: %d throttle: %d\n", 
                   sc->current_temp, sc->throttle_count);
    
    return uiomove(buf, min(len, uio->uio_resid), uio);
}

static int
thermal_probe(device_t dev)
{
    if (!(cpu_feature & CPUID_ACPI))
        return ENXIO;
        
    device_set_desc(dev, "CPU Thermal Monitor");
    return BUS_PROBE_DEFAULT;
}

static int
thermal_attach(device_t dev)
{
    struct thermal_softc *sc = device_get_softc(dev);
    
    sc->dev = dev;
    sc->critical_temp = 85;
    sc->throttle_count = 0;
    mtx_init(&sc->mtx, "thermal", NULL, MTX_DEF);
    
    sysctl_ctx_init(&sc->sysctl_ctx);
    sc->sysctl_tree = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
        SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO, "thermal",
        CTLFLAG_RD, 0, "Thermal monitoring");
    
    SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
        OID_AUTO, "temperature", CTLTYPE_INT | CTLFLAG_RD,
        sc, 0, thermal_temp_sysctl, "I", "Current CPU temperature");
    
    SYSCTL_ADD_PROC(&sc->sysctl_ctx, SYSCTL_CHILDREN(sc->sysctl_tree),
        OID_AUTO, "throttle_count", CTLTYPE_INT | CTLFLAG_RD,
        sc, 0, thermal_throttle_sysctl, "I", "Thermal throttle events");
    
    sc->cdev = make_dev(&thermal_cdevsw, 0, UID_ROOT, GID_WHEEL, 0644, "thermal");
    sc->cdev->si_drv1 = sc;
    
    return 0;
}

static int
thermal_detach(device_t dev)
{
    struct thermal_softc *sc = device_get_softc(dev);
    
    if (sc->cdev)
        destroy_dev(sc->cdev);
    sysctl_ctx_free(&sc->sysctl_ctx);
    mtx_destroy(&sc->mtx);
    
    return 0;
}

static device_method_t thermal_methods[] = {
    DEVMETHOD(device_probe, thermal_probe),
    DEVMETHOD(device_attach, thermal_attach),
    DEVMETHOD(device_detach, thermal_detach),
    DEVMETHOD_END
};

static driver_t thermal_driver = {
    "thermal",
    thermal_methods,
    sizeof(struct thermal_softc)
};

static devclass_t thermal_devclass;

DRIVER_MODULE(thermal, nexus, thermal_driver, thermal_devclass, 0, 0);
MODULE_VERSION(thermal, 1);
MODULE_DEPEND(thermal, nexus, 1, 1, 1);
