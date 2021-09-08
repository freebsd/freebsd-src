// cc_fuzz_target test for sshsig verification.

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern "C" {

#include "includes.h"
#include "sshkey.h"
#include "ssherr.h"
#include "sshbuf.h"
#include "sshsig.h"
#include "log.h"

int LLVMFuzzerTestOneInput(const uint8_t* sig, size_t slen)
{
  static const char *data = "If everyone started announcing his nose had "
      "run away, I don’t know how it would all end";
  struct sshbuf *signature = sshbuf_from(sig, slen);
  struct sshbuf *message = sshbuf_from(data, strlen(data));
  struct sshkey *k = NULL;
  struct sshkey_sig_details *details = NULL;
  extern char *__progname;

  log_init(__progname, SYSLOG_LEVEL_QUIET, SYSLOG_FACILITY_USER, 1);
  sshsig_verifyb(signature, message, "castle", &k, &details);
  sshkey_sig_details_free(details);
  sshkey_free(k);
  sshbuf_free(signature);
  sshbuf_free(message);
  return 0;
}

} // extern
