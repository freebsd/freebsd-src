#include <dlfcn.h>
#include <errno.h>
#include <krb5_locl.h>
#include <stdio.h>
#include <openssl/provider.h>

#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
static void fbsd_ossl_provider_unload(void);
static void print_dlerror(char *);
static OSSL_PROVIDER *legacy;
static OSSL_PROVIDER *deflt;
static int providers_loaded = 0;
static OSSL_PROVIDER * (*ossl_provider_load)(OSSL_LIB_CTX *, const char*) = NULL;
static int (*ossl_provider_unload)(OSSL_PROVIDER *) = NULL;
static void *crypto_lib_handle = NULL;

static void
fbsd_ossl_provider_unload(void)
{
	if (ossl_provider_unload == NULL) {
		if (!(ossl_provider_unload = (int (*)(OSSL_PROVIDER*)) dlsym(crypto_lib_handle, "OSSL_PROVIDER_unload"))) {
			print_dlerror("Unable to link OSSL_PROVIDER_unload");
			return;
		}
	}
	if (providers_loaded == 1) {
		(*ossl_provider_unload)(legacy);
		(*ossl_provider_unload)(deflt);
		providers_loaded = 0;
	}
}

static void
print_dlerror(char *message)
{
	char *errstr;

	if ((errstr = dlerror()) != NULL)
		fprintf(stderr, "%s: %s\n",
			message, errstr);
}
#endif

int
fbsd_ossl_provider_load(void)
{
#if defined(OPENSSL_VERSION_MAJOR) && (OPENSSL_VERSION_MAJOR >= 3)
	if (crypto_lib_handle == NULL) {
		if (!(crypto_lib_handle = dlopen("/usr/lib/libcrypto.so",
		    RTLD_LAZY|RTLD_GLOBAL))) {
			print_dlerror("Unable to load libcrypto.so");
			return (EINVAL);
		}
	}
	if (ossl_provider_load == NULL) {
		if (!(ossl_provider_load = (OSSL_PROVIDER * (*)(OSSL_LIB_CTX*, const char *)) dlsym(crypto_lib_handle, "OSSL_PROVIDER_load"))) {
			print_dlerror("Unable to link OSSL_PROVIDER_load");
			return(ENOENT);
		}
	}

	if (providers_loaded == 0) {
		if ((legacy = (*ossl_provider_load)(NULL, "legacy")) == NULL)
			return (EINVAL);
		if ((deflt = (*ossl_provider_load)(NULL, "default")) == NULL) {
			(*ossl_provider_unload)(legacy);
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
