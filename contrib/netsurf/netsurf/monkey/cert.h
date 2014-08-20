void 
gui_cert_verify(nsurl *url, const struct ssl_cert_info *certs, 
                unsigned long num, nserror (*cb)(bool proceed, void *pw),
                void *cbpw);
