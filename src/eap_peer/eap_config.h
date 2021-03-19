/*
 * EAP peer configuration data
 * Copyright (c) 2003-2019, Jouni Malinen <j@w1.fi>
 *
 * This software may be distributed under the terms of the BSD license.
 * See README for more details.
 */

#ifndef EAP_CONFIG_H
#define EAP_CONFIG_H

/**
 * struct eap_peer_cert_config - EAP peer certificate configuration/credential
 */
struct eap_peer_cert_config {
	/**
	 * ca_cert - File path to CA certificate file (PEM/DER)
	 *
	 * This file can have one or more trusted CA certificates. If ca_cert
	 * and ca_path are not included, server certificate will not be
	 * verified. This is insecure and a trusted CA certificate should
	 * always be configured when using EAP-TLS/TTLS/PEAP. Full path to the
	 * file should be used since working directory may change when
	 * wpa_supplicant is run in the background.
	 *
	 * Alternatively, a named configuration blob can be used by setting
	 * this to blob://blob_name.
	 *
	 * Alternatively, this can be used to only perform matching of the
	 * server certificate (SHA-256 hash of the DER encoded X.509
	 * certificate). In this case, the possible CA certificates in the
	 * server certificate chain are ignored and only the server certificate
	 * is verified. This is configured with the following format:
	 * hash:://server/sha256/cert_hash_in_hex
	 * For example: "hash://server/sha256/
	 * 5a1bc1296205e6fdbe3979728efe3920798885c1c4590b5f90f43222d239ca6a"
	 *
	 * On Windows, trusted CA certificates can be loaded from the system
	 * certificate store by setting this to cert_store://name, e.g.,
	 * ca_cert="cert_store://CA" or ca_cert="cert_store://ROOT".
	 * Note that when running wpa_supplicant as an application, the user
	 * certificate store (My user account) is used, whereas computer store
	 * (Computer account) is used when running wpasvc as a service.
	 */
	char *ca_cert;

	/**
	 * ca_path - Directory path for CA certificate files (PEM)
	 *
	 * This path may contain multiple CA certificates in OpenSSL format.
	 * Common use for this is to point to system trusted CA list which is
	 * often installed into directory like /etc/ssl/certs. If configured,
	 * these certificates are added to the list of trusted CAs. ca_cert
	 * may also be included in that case, but it is not required.
	 */
	char *ca_path;

	/**
	 * client_cert - File path to client certificate file (PEM/DER)
	 *
	 * This field is used with EAP method that use TLS authentication.
	 * Usually, this is only configured for EAP-TLS, even though this could
	 * in theory be used with EAP-TTLS and EAP-PEAP, too. Full path to the
	 * file should be used since working directory may change when
	 * wpa_supplicant is run in the background.
	 *
	 * Alternatively, a named configuration blob can be used by setting
	 * this to blob://blob_name.
	 */
	char *client_cert;

	/**
	 * private_key - File path to client private key file (PEM/DER/PFX)
	 *
	 * When PKCS#12/PFX file (.p12/.pfx) is used, client_cert should be
	 * commented out. Both the private key and certificate will be read
	 * from the PKCS#12 file in this case. Full path to the file should be
	 * used since working directory may change when wpa_supplicant is run
	 * in the background.
	 *
	 * Windows certificate store can be used by leaving client_cert out and
	 * configuring private_key in one of the following formats:
	 *
	 * cert://substring_to_match
	 *
	 * hash://certificate_thumbprint_in_hex
	 *
	 * For example: private_key="hash://63093aa9c47f56ae88334c7b65a4"
	 *
	 * Note that when running wpa_supplicant as an application, the user
	 * certificate store (My user account) is used, whereas computer store
	 * (Computer account) is used when running wpasvc as a service.
	 *
	 * Alternatively, a named configuration blob can be used by setting
	 * this to blob://blob_name.
	 */
	char *private_key;

	/**
	 * private_key_passwd - Password for private key file
	 *
	 * If left out, this will be asked through control interface.
	 */
	char *private_key_passwd;

	/**
	 * dh_file - File path to DH/DSA parameters file (in PEM format)
	 *
	 * This is an optional configuration file for setting parameters for an
	 * ephemeral DH key exchange. In most cases, the default RSA
	 * authentication does not use this configuration. However, it is
	 * possible setup RSA to use ephemeral DH key exchange. In addition,
	 * ciphers with DSA keys always use ephemeral DH keys. This can be used
	 * to achieve forward secrecy. If the file is in DSA parameters format,
	 * it will be automatically converted into DH params. Full path to the
	 * file should be used since working directory may change when
	 * wpa_supplicant is run in the background.
	 *
	 * Alternatively, a named configuration blob can be used by setting
	 * this to blob://blob_name.
	 */
	char *dh_file;

	/**
	 * subject_match - Constraint for server certificate subject
	 *
	 * This substring is matched against the subject of the authentication
	 * server certificate. If this string is set, the server certificate is
	 * only accepted if it contains this string in the subject. The subject
	 * string is in following format:
	 *
	 * /C=US/ST=CA/L=San Francisco/CN=Test AS/emailAddress=as@n.example.com
	 *
	 * Note: Since this is a substring match, this cannot be used securely
	 * to do a suffix match against a possible domain name in the CN entry.
	 * For such a use case, domain_suffix_match should be used instead.
	 */
	char *subject_match;

	/**
	 * check_cert_subject - Constraint for server certificate subject fields
	 *
	 * If check_cert_subject is set, the value of every field will be
	 * checked against the DN of the subject in the authentication server
	 * certificate. If the values do not match, the certificate verification
	 * will fail, rejecting the server. This option allows wpa_supplicant to
	 * match every individual field in the right order against the DN of the
	 * subject in the server certificate.
	 *
	 * For example, check_cert_subject=C=US/O=XX/OU=ABC/OU=XYZ/CN=1234 will
	 * check every individual DN field of the subject in the server
	 * certificate. If OU=XYZ comes first in terms of the order in the
	 * server certificate (DN field of server certificate
	 * C=US/O=XX/OU=XYZ/OU=ABC/CN=1234), wpa_supplicant will reject the
	 * server because the order of 'OU' is not matching the specified string
	 * in check_cert_subject.
	 *
	 * This option also allows '*' as a wildcard. This option has some
	 * limitation.
	 * It can only be used as per the following example.
	 *
	 * For example, check_cert_subject=C=US/O=XX/OU=Production* and we have
	 * two servers and DN of the subject in the first server certificate is
	 * (C=US/O=XX/OU=Production Unit) and DN of the subject in the second
	 * server is (C=US/O=XX/OU=Production Factory). In this case,
	 * wpa_supplicant will allow both servers because the value of 'OU'
	 * field in both server certificates matches 'OU' value in
	 * 'check_cert_subject' up to 'wildcard'.
	 *
	 * (Allow all servers, e.g., check_cert_subject=*)
	 */
	char *check_cert_subject;

	/**
	 * altsubject_match - Constraint for server certificate alt. subject
	 *
	 * Semicolon separated string of entries to be matched against the
	 * alternative subject name of the authentication server certificate.
	 * If this string is set, the server certificate is only accepted if it
	 * contains one of the entries in an alternative subject name
	 * extension.
	 *
	 * altSubjectName string is in following format: TYPE:VALUE
	 *
	 * Example: EMAIL:server@example.com
	 * Example: DNS:server.example.com;DNS:server2.example.com
	 *
	 * Following types are supported: EMAIL, DNS, URI
	 */
	char *altsubject_match;

	/**
	 * domain_suffix_match - Constraint for server domain name
	 *
	 * If set, this semicolon deliminated list of FQDNs is used as suffix
	 * match requirements for the server certificate in SubjectAltName
	 * dNSName element(s). If a matching dNSName is found against any of the
	 * specified values, this constraint is met. If no dNSName values are
	 * present, this constraint is matched against SubjectName CN using same
	 * suffix match comparison. Suffix match here means that the host/domain
	 * name is compared case-insentively one label at a time starting from
	 * the top-level domain and all the labels in domain_suffix_match shall
	 * be included in the certificate. The certificate may include
	 * additional sub-level labels in addition to the required labels.
	 *
	 * For example, domain_suffix_match=example.com would match
	 * test.example.com but would not match test-example.com. Multiple
	 * match options can be specified in following manner:
	 * example.org;example.com.
	 */
	char *domain_suffix_match;

	/**
	 * domain_match - Constraint for server domain name
	 *
	 * If set, this FQDN is used as a full match requirement for the
	 * server certificate in SubjectAltName dNSName element(s). If a
	 * matching dNSName is found, this constraint is met. If no dNSName
	 * values are present, this constraint is matched against SubjectName CN
	 * using same full match comparison. This behavior is similar to
	 * domain_suffix_match, but has the requirement of a full match, i.e.,
	 * no subdomains or wildcard matches are allowed. Case-insensitive
	 * comparison is used, so "Example.com" matches "example.com", but would
	 * not match "test.Example.com".
	 *
	 * More than one match string can be provided by using semicolons to
	 * separate the strings (e.g., example.org;example.com). When multiple
	 * strings are specified, a match with any one of the values is
	 * considered a sufficient match for the certificate, i.e., the
	 * conditions are ORed together.
	 */
	char *domain_match;

	/**
	 * pin - PIN for USIM, GSM SIM, and smartcards
	 *
	 * This field is used to configure PIN for SIM and smartcards for
	 * EAP-SIM and EAP-AKA. In addition, this is used with EAP-TLS if a
	 * smartcard is used for private key operations.
	 *
	 * If left out, this will be asked through control interface.
	 */
	char *pin;

	/**
	 * engine - Enable OpenSSL engine (e.g., for smartcard access)
	 *
	 * This is used if private key operations for EAP-TLS are performed
	 * using a smartcard.
	 */
	int engine;

	/**
	 * engine_id - Engine ID for OpenSSL engine
	 *
	 * "opensc" to select OpenSC engine or "pkcs11" to select PKCS#11
	 * engine.
	 *
	 * This is used if private key operations for EAP-TLS are performed
	 * using a smartcard.
	 */
	char *engine_id;


	/**
	 * key_id - Key ID for OpenSSL engine
	 *
	 * This is used if private key operations for EAP-TLS are performed
	 * using a smartcard.
	 */
	char *key_id;

	/**
	 * cert_id - Cert ID for OpenSSL engine
	 *
	 * This is used if the certificate operations for EAP-TLS are performed
	 * using a smartcard.
	 */
	char *cert_id;

	/**
	 * ca_cert_id - CA Cert ID for OpenSSL engine
	 *
	 * This is used if the CA certificate for EAP-TLS is on a smartcard.
	 */
	char *ca_cert_id;

	/**
	 * ocsp - Whether to use/require OCSP to check server certificate
	 *
	 * 0 = do not use OCSP stapling (TLS certificate status extension)
	 * 1 = try to use OCSP stapling, but not require response
	 * 2 = require valid OCSP stapling response
	 */
	int ocsp;
};

/**
 * struct eap_peer_config - EAP peer configuration/credentials
 */
struct eap_peer_config {
	/**
	 * identity - EAP Identity
	 *
	 * This field is used to set the real user identity or NAI (for
	 * EAP-PSK/PAX/SAKE/GPSK).
	 */
	u8 *identity;

	/**
	 * identity_len - EAP Identity length
	 */
	size_t identity_len;

	/**
	 * anonymous_identity -  Anonymous EAP Identity
	 *
	 * This field is used for unencrypted use with EAP types that support
	 * different tunnelled identity, e.g., EAP-TTLS, in order to reveal the
	 * real identity (identity field) only to the authentication server.
	 *
	 * If not set, the identity field will be used for both unencrypted and
	 * protected fields.
	 *
	 * This field can also be used with EAP-SIM/AKA/AKA' to store the
	 * pseudonym identity.
	 */
	u8 *anonymous_identity;

	/**
	 * anonymous_identity_len - Length of anonymous_identity
	 */
	size_t anonymous_identity_len;

	u8 *imsi_identity;
	size_t imsi_identity_len;

	/**
	 * machine_identity - EAP Identity for machine credential
	 *
	 * This field is used to set the machine identity or NAI for cases where
	 * and explicit machine credential (instead of or in addition to a user
	 * credential (from %identity) is needed.
	 */
	u8 *machine_identity;

	/**
	 * machine_identity_len - EAP Identity length for machine credential
	 */
	size_t machine_identity_len;

	/**
	 * password - Password string for EAP
	 *
	 * This field can include either the plaintext password (default
	 * option) or a NtPasswordHash (16-byte MD4 hash of the unicode
	 * presentation of the password) if flags field has
	 * EAP_CONFIG_FLAGS_PASSWORD_NTHASH bit set to 1. NtPasswordHash can
	 * only be used with authentication mechanism that use this hash as the
	 * starting point for operation: MSCHAP and MSCHAPv2 (EAP-MSCHAPv2,
	 * EAP-TTLS/MSCHAPv2, EAP-TTLS/MSCHAP, LEAP).
	 *
	 * In addition, this field is used to configure a pre-shared key for
	 * EAP-PSK/PAX/SAKE/GPSK. The length of the PSK must be 16 for EAP-PSK
	 * and EAP-PAX and 32 for EAP-SAKE. EAP-GPSK can use a variable length
	 * PSK.
	 */
	u8 *password;

	/**
	 * password_len - Length of password field
	 */
	size_t password_len;

	/**
	 * machine_password - Password string for EAP machine credential
	 *
	 * This field is used when machine credential based on username/password
	 * is needed instead of a user credential (from %password). See
	 * %password for more details on the format.
	 */
	u8 *machine_password;

	/**
	 * machine_password_len - Length of machine credential password field
	 */
	size_t machine_password_len;

	/**
	 * cert - Certificate parameters for Phase 1
	 */
	struct eap_peer_cert_config cert;

	/**
	 * phase2_cert - Certificate parameters for Phase 2
	 *
	 * This is like cert, but used for Phase 2 (inside
	 * EAP-TTLS/PEAP/FAST/TEAP tunnel) authentication.
	 */
	struct eap_peer_cert_config phase2_cert;

	/**
	 * machine_cert - Certificate parameters for Phase 2 machine credential
	 *
	 * This is like cert, but used for Phase 2 (inside EAP-TEAP tunnel)
	 * authentication with machine credentials (while phase2_cert is used
	 * for user credentials).
	 */
	struct eap_peer_cert_config machine_cert;

	/**
	 * eap_methods - Allowed EAP methods
	 *
	 * (vendor=EAP_VENDOR_IETF,method=EAP_TYPE_NONE) terminated list of
	 * allowed EAP methods or %NULL if all methods are accepted.
	 */
	struct eap_method_type *eap_methods;

	/**
	 * phase1 - Phase 1 (outer authentication) parameters
	 *
	 * String with field-value pairs, e.g., "peapver=0" or
	 * "peapver=1 peaplabel=1".
	 *
	 * 'peapver' can be used to force which PEAP version (0 or 1) is used.
	 *
	 * 'peaplabel=1' can be used to force new label, "client PEAP
	 * encryption",	to be used during key derivation when PEAPv1 or newer.
	 *
	 * Most existing PEAPv1 implementation seem to be using the old label,
	 * "client EAP encryption", and wpa_supplicant is now using that as the
	 * default value.
	 *
	 * Some servers, e.g., Radiator, may require peaplabel=1 configuration
	 * to interoperate with PEAPv1; see eap_testing.txt for more details.
	 *
	 * 'peap_outer_success=0' can be used to terminate PEAP authentication
	 * on tunneled EAP-Success. This is required with some RADIUS servers
	 * that implement draft-josefsson-pppext-eap-tls-eap-05.txt (e.g.,
	 * Lucent NavisRadius v4.4.0 with PEAP in "IETF Draft 5" mode).
	 *
	 * include_tls_length=1 can be used to force wpa_supplicant to include
	 * TLS Message Length field in all TLS messages even if they are not
	 * fragmented.
	 *
	 * sim_min_num_chal=3 can be used to configure EAP-SIM to require three
	 * challenges (by default, it accepts 2 or 3).
	 *
	 * result_ind=1 can be used to enable EAP-SIM and EAP-AKA to use
	 * protected result indication.
	 *
	 * fast_provisioning option can be used to enable in-line provisioning
	 * of EAP-FAST credentials (PAC):
	 * 0 = disabled,
	 * 1 = allow unauthenticated provisioning,
	 * 2 = allow authenticated provisioning,
	 * 3 = allow both unauthenticated and authenticated provisioning
	 *
	 * fast_max_pac_list_len=num option can be used to set the maximum
	 * number of PAC entries to store in a PAC list (default: 10).
	 *
	 * fast_pac_format=binary option can be used to select binary format
	 * for storing PAC entries in order to save some space (the default
	 * text format uses about 2.5 times the size of minimal binary format).
	 *
	 * crypto_binding option can be used to control PEAPv0 cryptobinding
	 * behavior:
	 * 0 = do not use cryptobinding (default)
	 * 1 = use cryptobinding if server supports it
	 * 2 = require cryptobinding
	 *
	 * EAP-WSC (WPS) uses following options: pin=Device_Password and
	 * uuid=Device_UUID
	 *
	 * For wired IEEE 802.1X authentication, "allow_canned_success=1" can be
	 * used to configure a mode that allows EAP-Success (and EAP-Failure)
	 * without going through authentication step. Some switches use such
	 * sequence when forcing the port to be authorized/unauthorized or as a
	 * fallback option if the authentication server is unreachable. By
	 * default, wpa_supplicant discards such frames to protect against
	 * potential attacks by rogue devices, but this option can be used to
	 * disable that protection for cases where the server/authenticator does
	 * not need to be authenticated.
	 */
	char *phase1;

	/**
	 * phase2 - Phase2 (inner authentication with TLS tunnel) parameters
	 *
	 * String with field-value pairs, e.g., "auth=MSCHAPV2" for EAP-PEAP or
	 * "autheap=MSCHAPV2 autheap=MD5" for EAP-TTLS. "mschapv2_retry=0" can
	 * be used to disable MSCHAPv2 password retry in authentication failure
	 * cases.
	 */
	char *phase2;

	/**
	 * machine_phase2 - Phase2 parameters for machine credentials
	 *
	 * See phase2 for more details.
	 */
	char *machine_phase2;

	/**
	 * pcsc - Parameters for PC/SC smartcard interface for USIM and GSM SIM
	 *
	 * This field is used to configure PC/SC smartcard interface.
	 * Currently, the only configuration is whether this field is %NULL (do
	 * not use PC/SC) or non-NULL (e.g., "") to enable PC/SC.
	 *
	 * This field is used for EAP-SIM and EAP-AKA.
	 */
	char *pcsc;

	/**
	 * otp - One-time-password
	 *
	 * This field should not be set in configuration step. It is only used
	 * internally when OTP is entered through the control interface.
	 */
	u8 *otp;

	/**
	 * otp_len - Length of the otp field
	 */
	size_t otp_len;

	/**
	 * pending_req_identity - Whether there is a pending identity request
	 *
	 * This field should not be set in configuration step. It is only used
	 * internally when control interface is used to request needed
	 * information.
	 */
	int pending_req_identity;

	/**
	 * pending_req_password - Whether there is a pending password request
	 *
	 * This field should not be set in configuration step. It is only used
	 * internally when control interface is used to request needed
	 * information.
	 */
	int pending_req_password;

	/**
	 * pending_req_pin - Whether there is a pending PIN request
	 *
	 * This field should not be set in configuration step. It is only used
	 * internally when control interface is used to request needed
	 * information.
	 */
	int pending_req_pin;

	/**
	 * pending_req_new_password - Pending password update request
	 *
	 * This field should not be set in configuration step. It is only used
	 * internally when control interface is used to request needed
	 * information.
	 */
	int pending_req_new_password;

	/**
	 * pending_req_passphrase - Pending passphrase request
	 *
	 * This field should not be set in configuration step. It is only used
	 * internally when control interface is used to request needed
	 * information.
	 */
	int pending_req_passphrase;

	/**
	 * pending_req_sim - Pending SIM request
	 *
	 * This field should not be set in configuration step. It is only used
	 * internally when control interface is used to request needed
	 * information.
	 */
	int pending_req_sim;

	/**
	 * pending_req_otp - Whether there is a pending OTP request
	 *
	 * This field should not be set in configuration step. It is only used
	 * internally when control interface is used to request needed
	 * information.
	 */
	char *pending_req_otp;

	/**
	 * pending_req_otp_len - Length of the pending OTP request
	 */
	size_t pending_req_otp_len;

	/**
	 * pac_file - File path or blob name for the PAC entries (EAP-FAST)
	 *
	 * wpa_supplicant will need to be able to create this file and write
	 * updates to it when PAC is being provisioned or refreshed. Full path
	 * to the file should be used since working directory may change when
	 * wpa_supplicant is run in the background.
	 * Alternatively, a named configuration blob can be used by setting
	 * this to blob://blob_name.
	 */
	char *pac_file;

	/**
	 * mschapv2_retry - MSCHAPv2 retry in progress
	 *
	 * This field is used internally by EAP-MSCHAPv2 and should not be set
	 * as part of configuration.
	 */
	int mschapv2_retry;

	/**
	 * new_password - New password for password update
	 *
	 * This field is used during MSCHAPv2 password update. This is normally
	 * requested from the user through the control interface and not set
	 * from configuration.
	 */
	u8 *new_password;

	/**
	 * new_password_len - Length of new_password field
	 */
	size_t new_password_len;

	/**
	 * fragment_size - Maximum EAP fragment size in bytes (default 1398)
	 *
	 * This value limits the fragment size for EAP methods that support
	 * fragmentation (e.g., EAP-TLS and EAP-PEAP). This value should be set
	 * small enough to make the EAP messages fit in MTU of the network
	 * interface used for EAPOL. The default value is suitable for most
	 * cases.
	 */
	int fragment_size;

#define EAP_CONFIG_FLAGS_PASSWORD_NTHASH BIT(0)
#define EAP_CONFIG_FLAGS_EXT_PASSWORD BIT(1)
#define EAP_CONFIG_FLAGS_MACHINE_PASSWORD_NTHASH BIT(2)
#define EAP_CONFIG_FLAGS_EXT_MACHINE_PASSWORD BIT(3)
	/**
	 * flags - Network configuration flags (bitfield)
	 *
	 * This variable is used for internal flags to describe further details
	 * for the network parameters.
	 * bit 0 = password is represented as a 16-byte NtPasswordHash value
	 *         instead of plaintext password
	 * bit 1 = password is stored in external storage; the value in the
	 *         password field is the name of that external entry
	 * bit 2 = machine password is represented as a 16-byte NtPasswordHash
	 *         value instead of plaintext password
	 * bit 3 = machine password is stored in external storage; the value in
	 *         the password field is the name of that external entry
	 */
	u32 flags;

	/**
	 * external_sim_resp - Response from external SIM processing
	 *
	 * This field should not be set in configuration step. It is only used
	 * internally when control interface is used to request external
	 * SIM/USIM processing.
	 */
	char *external_sim_resp;

	/**
	 * sim_num - User selected SIM identifier
	 *
	 * This variable is used for identifying which SIM is used if the system
	 * has more than one.
	 */
	int sim_num;

	/**
	 * openssl_ciphers - OpenSSL cipher string
	 *
	 * This is an OpenSSL specific configuration option for configuring the
	 * ciphers for this connection. If not set, the default cipher suite
	 * list is used.
	 */
	char *openssl_ciphers;

	/**
	 * erp - Whether EAP Re-authentication Protocol (ERP) is enabled
	 */
	int erp;

	/**
	 * pending_ext_cert_check - External server certificate check status
	 *
	 * This field should not be set in configuration step. It is only used
	 * internally when control interface is used to request external
	 * validation of server certificate chain.
	 */
	enum {
		NO_CHECK = 0,
		PENDING_CHECK,
		EXT_CERT_CHECK_GOOD,
		EXT_CERT_CHECK_BAD,
	} pending_ext_cert_check;

	int teap_anon_dh;
};


/**
 * struct wpa_config_blob - Named configuration blob
 *
 * This data structure is used to provide storage for binary objects to store
 * abstract information like certificates and private keys inlined with the
 * configuration data.
 */
struct wpa_config_blob {
	/**
	 * name - Blob name
	 */
	char *name;

	/**
	 * data - Pointer to binary data
	 */
	u8 *data;

	/**
	 * len - Length of binary data
	 */
	size_t len;

	/**
	 * next - Pointer to next blob in the configuration
	 */
	struct wpa_config_blob *next;
};

#endif /* EAP_CONFIG_H */
