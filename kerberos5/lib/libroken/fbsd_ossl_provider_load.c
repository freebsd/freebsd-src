#include <errno.h>
#include <krb5_locl.h>

static void fbsd_ossl_provider_unload(void);

static OSSL_PROVIDER *legacy;
static OSSL_PROVIDER *deflt;
static int providers_loaded = 0;

int
fbsd_ossl_provider_load(void)
{
#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
	if (providers_loaded == 0) {
		if ((legacy = OSSL_PROVIDER_load(NULL, "legacy")) == NULL)
			return (EINVAL);
		if ((deflt = OSSL_PROVIDER_load(NULL, "default")) == NULL) {
			OSSL_PROVIDER_unload(legacy);
			return (EINVAL);
		}
		if (atexit(fbsd_ossl_provider_unload)) {
			fbsd_ossl_provider_unload();
			return (errno);
		}
		providers_loaded = 1;
	}
#endif
	return (0);
}

static void
fbsd_ossl_provider_unload(void)
{
#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
	if (providers_loaded == 1) {
		OSSL_PROVIDER_unload(legacy);
		OSSL_PROVIDER_unload(deflt);
		providers_loaded = 0;
	}
#endif
}
