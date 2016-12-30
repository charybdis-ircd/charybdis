/*
 *  libratbox: a library used by ircd-ratbox and other things
 *  gnutls.c: gnutls related code
 *
 *  Copyright (C) 2007-2008 ircd-ratbox development team
 *  Copyright (C) 2007-2008 Aaron Sethman <androsyn@ratbox.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 *
 */

#include <librb_config.h>
#include <rb_lib.h>

#ifdef HAVE_GNUTLS

#include <commio-int.h>
#include <commio-ssl.h>
#include <stdbool.h>

#include <gnutls/gnutls.h>

#include <gnutls/abstract.h>
#include <gnutls/x509.h>

#if (GNUTLS_VERSION_MAJOR < 3)
# include <gcrypt.h>
#else
# include <gnutls/crypto.h>
#endif

#include "gnutls_ratbox.h"

typedef enum
{
	RB_FD_TLS_DIRECTION_IN = 0,
	RB_FD_TLS_DIRECTION_OUT = 1
} rb_fd_tls_direction;

#define SSL_P(x) *((gnutls_session_t *) ((x)->ssl))



// Server side variables
static gnutls_certificate_credentials_t server_cert_key;
static gnutls_dh_params_t server_dhp;

// Client side variables
#define MAX_CERTS 6
static gnutls_x509_crt_t client_cert[MAX_CERTS];
static gnutls_x509_privkey_t client_key;
static unsigned int client_cert_count;

// Shared variables
static gnutls_priority_t default_priority;



struct ssl_connect
{
	CNCB *callback;
	void *data;
	int timeout;
};

static const char *rb_ssl_strerror(int);
static void rb_ssl_connect_realcb(rb_fde_t *, int, struct ssl_connect *);



/*
 * Internal GNUTLS-specific code
 */

/*
 * We only have one certificate to authenticate with, as both a client and server.
 *
 * Unfortunately, GNUTLS tries to be clever, and as client, will attempt to use a certificate that the server will
 * trust. We usually use self-signed certs, though, so the result of this search is always nothing. Therefore, it
 * uses no certificate to authenticate as a client. This is undesirable, as it breaks fingerprint authentication;
 * e.g. the connect::fingerprint on the remote ircd will not match.
 *
 * Thus, we use this callback to force GNUTLS to authenticate with our (server) certificate as a client.
 */
static int
rb_ssl_cert_auth_cb(gnutls_session_t session,
                    const gnutls_datum_t *const req_ca_rdn, const int req_ca_rdn_len,
                    const gnutls_pk_algorithm_t *const sign_algos, const int sign_algos_len,
#if (GNUTLS_VERSION_MAJOR < 3)
                    gnutls_retr_st *const st)
#else
                    gnutls_retr2_st *const st)
#endif
{
#if (GNUTLS_VERSION_MAJOR < 3)
	st->type = GNUTLS_CRT_X509;
#else
	st->cert_type = GNUTLS_CRT_X509;
	st->key_type = GNUTLS_PRIVKEY_X509;
#endif

	st->ncerts = client_cert_count;
	st->cert.x509 = client_cert;
	st->key.x509 = client_key;
	st->deinit_all = 0;

	return 0;
}

static ssize_t
rb_sock_net_recv(gnutls_transport_ptr_t context_ptr, void *const buf, const size_t count)
{
	const int fd = rb_get_fd((rb_fde_t *)context_ptr);

	return recv(fd, buf, count, 0);
}

static ssize_t
rb_sock_net_xmit(gnutls_transport_ptr_t context_ptr, const void *const buf, const size_t count)
{
	const int fd = rb_get_fd((rb_fde_t *)context_ptr);

	return send(fd, buf, count, 0);
}

static void
rb_ssl_init_fd(rb_fde_t *const F, const rb_fd_tls_direction dir)
{
	F->ssl = rb_malloc(sizeof(gnutls_session_t));

	if(F->ssl == NULL)
	{
		rb_lib_log("%s: rb_malloc: allocation failure", __func__);
		rb_close(F);
		return;
	}

	unsigned int init_flags = 0;

	switch(dir)
	{
	case RB_FD_TLS_DIRECTION_IN:
		init_flags |= GNUTLS_SERVER;
		break;
	case RB_FD_TLS_DIRECTION_OUT:
		init_flags |= GNUTLS_CLIENT;
		break;
	}

	gnutls_init((gnutls_session_t *) F->ssl, init_flags);
	gnutls_credentials_set(SSL_P(F), GNUTLS_CRD_CERTIFICATE, server_cert_key);
	gnutls_dh_set_prime_bits(SSL_P(F), 2048);

	gnutls_transport_set_ptr(SSL_P(F), (gnutls_transport_ptr_t) F);
	gnutls_transport_set_pull_function(SSL_P(F), rb_sock_net_recv);
	gnutls_transport_set_push_function(SSL_P(F), rb_sock_net_xmit);

	if (gnutls_priority_set(SSL_P(F), default_priority) != GNUTLS_E_SUCCESS)
		gnutls_set_default_priority(SSL_P(F));

	if(dir == RB_FD_TLS_DIRECTION_IN)
		gnutls_certificate_server_set_request(SSL_P(F), GNUTLS_CERT_REQUEST);
}

static void
rb_ssl_accept_common(rb_fde_t *const F, void *const data)
{
	lrb_assert(F != NULL);
	lrb_assert(F->accept != NULL);
	lrb_assert(F->accept->callback != NULL);
	lrb_assert(F->ssl != NULL);

	errno = 0;

	const int ret = gnutls_handshake(SSL_P(F));
	const int err = errno;

	if(ret == GNUTLS_E_AGAIN || (ret == GNUTLS_E_INTERRUPTED && (err == 0 || rb_ignore_errno(err))))
	{
		unsigned int flags = (gnutls_record_get_direction(SSL_P(F)) == 0) ? RB_SELECT_READ : RB_SELECT_WRITE;
		rb_setselect(F, flags, rb_ssl_accept_common, data);
		return;
	}

	// These 2 calls may affect errno, which is why we save it above and restore it below
	rb_settimeout(F, 0, NULL, NULL);
	rb_setselect(F, RB_SELECT_READ | RB_SELECT_WRITE, NULL, NULL);

	struct acceptdata *const ad = F->accept;
	F->accept = NULL;

	if(ret == GNUTLS_E_SUCCESS)
	{
		F->handshake_count++;
		ad->callback(F, RB_OK, (struct sockaddr *)&ad->S, ad->addrlen, ad->data);
	}
	else if(ret == GNUTLS_E_INTERRUPTED && err != 0)
	{
		errno = err;
		ad->callback(F, RB_ERROR, NULL, 0, ad->data);
	}
	else
	{
		errno = EIO;
		F->ssl_errno = (unsigned long) -ret;
		ad->callback(F, RB_ERROR_SSL, NULL, 0, ad->data);
	}

	rb_free(ad);
}

static void
rb_ssl_connect_common(rb_fde_t *const F, void *const data)
{
	lrb_assert(F != NULL);
	lrb_assert(F->ssl != NULL);

	errno = 0;

	const int ret = gnutls_handshake(SSL_P(F));
	const int err = errno;

	if(ret == GNUTLS_E_AGAIN || (ret == GNUTLS_E_INTERRUPTED && (err == 0 || rb_ignore_errno(err))))
	{
		unsigned int flags = (gnutls_record_get_direction(SSL_P(F)) == 0) ? RB_SELECT_READ : RB_SELECT_WRITE;
		rb_setselect(F, flags, rb_ssl_connect_common, data);
		return;
	}

	// These 2 calls may affect errno, which is why we save it above and restore it below
	rb_settimeout(F, 0, NULL, NULL);
	rb_setselect(F, RB_SELECT_READ | RB_SELECT_WRITE, NULL, NULL);

	struct ssl_connect *const sconn = data;

	if(ret == GNUTLS_E_SUCCESS)
	{
		F->handshake_count++;
		rb_ssl_connect_realcb(F, RB_OK, sconn);
	}
	else if(ret == GNUTLS_E_INTERRUPTED && err != 0)
	{
		errno = err;
		rb_ssl_connect_realcb(F, RB_ERROR, sconn);
	}
	else
	{
		errno = EIO;
		F->ssl_errno = (unsigned long) -ret;
		rb_ssl_connect_realcb(F, RB_ERROR_SSL, sconn);
	}
}

static const char *
rb_ssl_strerror(const int err)
{
	return gnutls_strerror(err);
}

static ssize_t
rb_ssl_read_or_write(const int r_or_w, rb_fde_t *const F, void *const rbuf, const void *const wbuf, const size_t count)
{
	ssize_t ret;

	errno = 0;

	if(r_or_w == 0)
		ret = gnutls_record_recv(SSL_P(F), rbuf, count);
	else
		ret = gnutls_record_send(SSL_P(F), wbuf, count);

	if(ret >= 0)
		return ret;

	if(ret == GNUTLS_E_AGAIN || (ret == GNUTLS_E_INTERRUPTED && (errno == 0 || rb_ignore_errno(errno))))
	{
		if(gnutls_record_get_direction(SSL_P(F)) == 0)
			return RB_RW_SSL_NEED_READ;
		else
			return RB_RW_SSL_NEED_WRITE;
	}

	if(ret == GNUTLS_E_INTERRUPTED && errno != 0)
		return RB_RW_IO_ERROR;

	errno = EIO;
	F->ssl_errno = (unsigned long) -ret;
	return RB_RW_SSL_ERROR;
}

#if (GNUTLS_VERSION_MAJOR < 3)
static void
rb_gcry_random_seed(void *const unused)
{
	gcry_fast_random_poll();
}
#endif

static void
rb_free_datum_t(gnutls_datum_t *const datum)
{
	if(datum == NULL)
		return;

	rb_free(datum->data);
	rb_free(datum);
}

static gnutls_datum_t *
rb_load_file_into_datum_t(const char *const file)
{
	const int datum_fd = open(file, O_RDONLY);
	if(datum_fd < 0)
		return NULL;

	struct stat fileinfo;
	if(fstat(datum_fd, &fileinfo) != 0)
	{
		(void) close(datum_fd);
		return NULL;
	}

	const size_t datum_size = (fileinfo.st_size < 131072) ? (size_t) fileinfo.st_size : 131072;
	if(datum_size == 0)
	{
		(void) close(datum_fd);
		return NULL;
	}

	gnutls_datum_t *datum;
	if((datum = rb_malloc(sizeof *datum)) == NULL)
	{
		(void) close(datum_fd);
		return NULL;
	}
	if((datum->data = rb_malloc(datum_size + 1)) == NULL)
	{
		rb_free(datum);
		(void) close(datum_fd);
		return NULL;
	}

	for(size_t data_read = 0; data_read < datum_size; )
	{
		ssize_t ret = read(datum_fd, ((unsigned char *)datum->data) + data_read, datum_size - data_read);

		if(ret <= 0)
		{
			rb_free_datum_t(datum);
			(void) close(datum_fd);
			return NULL;
		}

		data_read += (size_t) ret;
	}
	(void) close(datum_fd);

	datum->data[datum_size] = '\0';
	datum->size = (unsigned int) datum_size;
	return datum;
}

static int
make_certfp(gnutls_x509_crt_t cert, uint8_t certfp[const RB_SSL_CERTFP_LEN], const int method)
{
	int hashlen;
	gnutls_digest_algorithm_t md_type;

	bool spki = false;

	switch(method)
	{
	case RB_SSL_CERTFP_METH_CERT_SHA1:
		hashlen = RB_SSL_CERTFP_LEN_SHA1;
		md_type = GNUTLS_DIG_SHA1;
		break;
	case RB_SSL_CERTFP_METH_SPKI_SHA256:
		spki = true;
	case RB_SSL_CERTFP_METH_CERT_SHA256:
		hashlen = RB_SSL_CERTFP_LEN_SHA256;
		md_type = GNUTLS_DIG_SHA256;
		break;
	case RB_SSL_CERTFP_METH_SPKI_SHA512:
		spki = true;
	case RB_SSL_CERTFP_METH_CERT_SHA512:
		hashlen = RB_SSL_CERTFP_LEN_SHA512;
		md_type = GNUTLS_DIG_SHA512;
		break;
	default:
		return 0;
	}

	if(! spki)
	{
		size_t digest_size = (size_t) hashlen;

		if(gnutls_x509_crt_get_fingerprint(cert, md_type, certfp, &digest_size) != 0)
			return 0;

		return hashlen;
	}

	gnutls_pubkey_t pubkey;

	if(gnutls_pubkey_init(&pubkey) != 0)
		return 0;

	if(gnutls_pubkey_import_x509(pubkey, cert, 0) != 0)
	{
		gnutls_pubkey_deinit(pubkey);
		return 0;
	}

	unsigned char derkey[262144];	// Should be big enough to hold any SubjectPublicKeyInfo structure
	size_t derkey_len = sizeof derkey;

	if(gnutls_pubkey_export(pubkey, GNUTLS_X509_FMT_DER, derkey, &derkey_len) != 0)
	{
		gnutls_pubkey_deinit(pubkey);
		return 0;
	}

	gnutls_pubkey_deinit(pubkey);

	if(gnutls_hash_fast(md_type, derkey, derkey_len, certfp) != 0)
		return 0;

	return hashlen;
}



/*
 * External GNUTLS-specific code
 */

void
rb_ssl_shutdown(rb_fde_t *const F)
{
	if(F == NULL || F->ssl == NULL)
		return;

	for(int i = 0; i < 4; i++)
	{
		int ret = gnutls_bye(SSL_P(F), GNUTLS_SHUT_RDWR);

		if(ret != GNUTLS_E_INTERRUPTED && ret != GNUTLS_E_AGAIN)
			break;
	}

	gnutls_deinit(SSL_P(F));

	rb_free(F->ssl);
	F->ssl = NULL;
}

int
rb_init_ssl(void)
{
	int ret;

	if((ret = gnutls_global_init()) != GNUTLS_E_SUCCESS)
	{
		rb_lib_log("%s: gnutls_global_init: %s", __func__, rb_ssl_strerror(ret));
		return 0;
	}

#if (GNUTLS_VERSION_MAJOR < 3)
	rb_event_addish("rb_gcry_random_seed", rb_gcry_random_seed, NULL, 300);
#endif

	return 1;
}

int
rb_setup_ssl_server(const char *const certfile, const char *keyfile,
                    const char *const dhfile, const char *cipherlist)
{
	if(certfile == NULL)
	{
		rb_lib_log("%s: no certificate file specified", __func__);
		return 0;
	}

	if(keyfile == NULL)
		keyfile = certfile;

	if(cipherlist == NULL)
		cipherlist = rb_gnutls_default_priority_str;


	gnutls_datum_t *const d_cert = rb_load_file_into_datum_t(certfile);
	if(d_cert == NULL)
	{
		rb_lib_log("%s: Error loading certificate: %s", __func__, strerror(errno));
		return 0;
	}

	gnutls_datum_t *const d_key = rb_load_file_into_datum_t(keyfile);
	if(d_key == NULL)
	{
		rb_lib_log("%s: Error loading key: %s", __func__, strerror(errno));
		rb_free_datum_t(d_cert);
		return 0;
	}

	int ret;

	if((ret = gnutls_certificate_allocate_credentials(&server_cert_key)) != GNUTLS_E_SUCCESS)
	{
		rb_lib_log("%s: gnutls_certificate_allocate_credentials: %s", __func__, rb_ssl_strerror(ret));
		rb_free_datum_t(d_cert);
		rb_free_datum_t(d_key);
		return 0;
	}

#if (GNUTLS_VERSION_MAJOR < 3)
	gnutls_certificate_client_set_retrieve_function(server_cert_key, rb_ssl_cert_auth_cb);
#else
	gnutls_certificate_set_retrieve_function(server_cert_key, rb_ssl_cert_auth_cb);
#endif

	if((ret = gnutls_certificate_set_x509_key_mem(server_cert_key, d_cert, d_key,
	                                              GNUTLS_X509_FMT_PEM)) != GNUTLS_E_SUCCESS)
	{
		rb_lib_log("%s: gnutls_certificate_set_x509_key_mem: %s", __func__, rb_ssl_strerror(ret));
		gnutls_certificate_free_credentials(server_cert_key);
		rb_free_datum_t(d_cert);
		rb_free_datum_t(d_key);
		return 0;
	}
	if((ret = gnutls_x509_crt_list_import(client_cert, &client_cert_count, d_cert, GNUTLS_X509_FMT_PEM,
	                                      GNUTLS_X509_CRT_LIST_IMPORT_FAIL_IF_EXCEED)) < 1)
	{
		rb_lib_log("%s: gnutls_x509_crt_list_import: %s", __func__, rb_ssl_strerror(ret));
		gnutls_certificate_free_credentials(server_cert_key);
		rb_free_datum_t(d_cert);
		rb_free_datum_t(d_key);
		return 0;
	}
	client_cert_count = (unsigned int) ret;

	if((ret = gnutls_x509_privkey_init(&client_key)) != GNUTLS_E_SUCCESS)
	{
		rb_lib_log("%s: gnutls_x509_privkey_init: %s", __func__, rb_ssl_strerror(ret));
		gnutls_certificate_free_credentials(server_cert_key);
		for(unsigned int i = 0; i < client_cert_count; i++)
			gnutls_x509_crt_deinit(client_cert[i]);
		rb_free_datum_t(d_cert);
		rb_free_datum_t(d_key);
		return 0;
	}
	if((ret = gnutls_x509_privkey_import(client_key, d_key, GNUTLS_X509_FMT_PEM)) != GNUTLS_E_SUCCESS)
	{
		rb_lib_log("%s: gnutls_x509_privkey_import: %s", __func__, rb_ssl_strerror(ret));
		gnutls_certificate_free_credentials(server_cert_key);
		for(unsigned int i = 0; i < client_cert_count; i++)
			gnutls_x509_crt_deinit(client_cert[i]);
		gnutls_x509_privkey_deinit(client_key);
		rb_free_datum_t(d_cert);
		rb_free_datum_t(d_key);
		return 0;
	}

	rb_free_datum_t(d_cert);
	rb_free_datum_t(d_key);

	if(dhfile != NULL)
	{
		gnutls_datum_t *const d_dhp = rb_load_file_into_datum_t(dhfile);

		if(d_dhp == NULL)
		{
			rb_lib_log("%s: Error parsing DH parameters: %s", __func__, strerror(errno));
			gnutls_certificate_free_credentials(server_cert_key);
			for(unsigned int i = 0; i < client_cert_count; i++)
				gnutls_x509_crt_deinit(client_cert[i]);
			gnutls_x509_privkey_deinit(client_key);
			return 0;
		}
		if((ret = gnutls_dh_params_init(&server_dhp)) != GNUTLS_E_SUCCESS)
		{
			rb_lib_log("%s: Error parsing DH parameters: %s", __func__, rb_ssl_strerror(ret));
			gnutls_certificate_free_credentials(server_cert_key);
			for(unsigned int i = 0; i < client_cert_count; i++)
				gnutls_x509_crt_deinit(client_cert[i]);
			gnutls_x509_privkey_deinit(client_key);
			rb_free_datum_t(d_dhp);
			return 0;
		}
		if((ret = gnutls_dh_params_import_pkcs3(server_dhp, d_dhp, GNUTLS_X509_FMT_PEM)) != GNUTLS_E_SUCCESS)
		{
			rb_lib_log("%s: Error parsing DH parameters: %s", __func__, rb_ssl_strerror(ret));
			gnutls_certificate_free_credentials(server_cert_key);
			for(unsigned int i = 0; i < client_cert_count; i++)
				gnutls_x509_crt_deinit(client_cert[i]);
			gnutls_x509_privkey_deinit(client_key);
			gnutls_dh_params_deinit(server_dhp);
			rb_free_datum_t(d_dhp);
			return 0;
		}

		gnutls_certificate_set_dh_params(server_cert_key, server_dhp);
		rb_free_datum_t(d_dhp);
	}

	const char *err = NULL;
	if((ret = gnutls_priority_init(&default_priority, cipherlist, &err)) != GNUTLS_E_SUCCESS)
	{
		rb_lib_log("%s: gnutls_priority_init: %s, error begins at '%s'? -- using defaults instead",
		           __func__, rb_ssl_strerror(ret), err ? err : "<unknown>");

		(void) gnutls_priority_init(&default_priority, NULL, &err);
	}

	rb_lib_log("%s: TLS configuration successful", __func__);
	return 1;
}

int
rb_init_prng(const char *const path, prng_seed_t seed_type)
{
#if (GNUTLS_VERSION_MAJOR < 3)
	gcry_fast_random_poll();
	rb_lib_log("%s: PRNG initialised", __func__);
#else
	rb_lib_log("%s: Skipping PRNG initialisation; not required by GNUTLS v3+ backend", __func__);
#endif
	return 1;
}

int
rb_get_random(void *const buf, const size_t length)
{
#if (GNUTLS_VERSION_MAJOR < 3)
	gcry_randomize(buf, length, GCRY_STRONG_RANDOM);
#else
	gnutls_rnd(GNUTLS_RND_KEY, buf, length);
#endif
	return 1;
}

const char *
rb_get_ssl_strerror(rb_fde_t *const F)
{
	const int err = (int) F->ssl_errno;
	return rb_ssl_strerror(-err);
}

int
rb_get_ssl_certfp(rb_fde_t *const F, uint8_t certfp[const RB_SSL_CERTFP_LEN], const int method)
{
	if(gnutls_certificate_type_get(SSL_P(F)) != GNUTLS_CRT_X509)
		return 0;

	unsigned int cert_list_size = 0;
	const gnutls_datum_t *const cert_list = gnutls_certificate_get_peers(SSL_P(F), &cert_list_size);
	if(cert_list == NULL || cert_list_size < 1)
		return 0;

	gnutls_x509_crt_t peer_cert;
	if(gnutls_x509_crt_init(&peer_cert) != 0)
		return 0;

	if(gnutls_x509_crt_import(peer_cert, &cert_list[0], GNUTLS_X509_FMT_DER) < 0)
	{
		gnutls_x509_crt_deinit(peer_cert);
		return 0;
	}

	const int len = make_certfp(peer_cert, certfp, method);

	gnutls_x509_crt_deinit(peer_cert);

	return len;
}

int
rb_get_ssl_certfp_file(const char *const filename, uint8_t certfp[const RB_SSL_CERTFP_LEN], const int method)
{
	gnutls_datum_t *const datum_cert = rb_load_file_into_datum_t(filename);
	if(datum_cert == NULL)
		return -1;

	gnutls_x509_crt_t cert;
	if(gnutls_x509_crt_init(&cert) != 0)
	{
		rb_free_datum_t(datum_cert);
		return 0;
	}
	if(gnutls_x509_crt_import(cert, datum_cert, GNUTLS_X509_FMT_PEM) < 0)
	{
		gnutls_x509_crt_deinit(cert);
		rb_free_datum_t(datum_cert);
		return 0;
	}

	const int len = make_certfp(cert, certfp, method);

	gnutls_x509_crt_deinit(cert);
	rb_free_datum_t(datum_cert);

	return len;
}

void
rb_get_ssl_info(char *const buf, const size_t len)
{
	(void) snprintf(buf, len, "GNUTLS: compiled (v%s), library (v%s)",
	                LIBGNUTLS_VERSION, gnutls_check_version(NULL));
}

const char *
rb_ssl_get_cipher(rb_fde_t *const F)
{
	if(F == NULL || F->ssl == NULL)
		return NULL;

	static char buf[512];

	gnutls_protocol_t version_ptr = gnutls_protocol_get_version(SSL_P(F));
	gnutls_cipher_algorithm_t cipher_ptr = gnutls_cipher_get(SSL_P(F));

	const char *const version = gnutls_protocol_get_name(version_ptr);
	const char *const cipher = gnutls_cipher_get_name(cipher_ptr);

	(void) snprintf(buf, sizeof buf, "%s, %s", version, cipher);

	return buf;
}

ssize_t
rb_ssl_read(rb_fde_t *const F, void *const buf, const size_t count)
{
	return rb_ssl_read_or_write(0, F, buf, NULL, count);
}

ssize_t
rb_ssl_write(rb_fde_t *const F, const void *const buf, const size_t count)
{
	return rb_ssl_read_or_write(1, F, NULL, buf, count);
}



/*
 * Internal library-agnostic code
 */

static void
rb_ssl_connect_realcb(rb_fde_t *const F, const int status, struct ssl_connect *const sconn)
{
	lrb_assert(F != NULL);
	lrb_assert(F->connect != NULL);

	F->connect->callback = sconn->callback;
	F->connect->data = sconn->data;

	rb_connect_callback(F, status);
	rb_free(sconn);
}

static void
rb_ssl_timeout_cb(rb_fde_t *const F, void *const data)
{
	lrb_assert(F->accept != NULL);
	lrb_assert(F->accept->callback != NULL);

	F->accept->callback(F, RB_ERR_TIMEOUT, NULL, 0, F->accept->data);
}

static void
rb_ssl_tryconn_timeout_cb(rb_fde_t *const F, void *const data)
{
	rb_ssl_connect_realcb(F, RB_ERR_TIMEOUT, data);
}

static void
rb_ssl_tryconn(rb_fde_t *const F, const int status, void *const data)
{
	lrb_assert(F != NULL);

	struct ssl_connect *const sconn = data;

	if(status != RB_OK)
	{
		rb_ssl_connect_realcb(F, status, sconn);
		return;
	}

	F->type |= RB_FD_SSL;

	rb_settimeout(F, sconn->timeout, rb_ssl_tryconn_timeout_cb, sconn);
	rb_ssl_init_fd(F, RB_FD_TLS_DIRECTION_OUT);
	rb_ssl_connect_common(F, sconn);
}



/*
 * External library-agnostic code
 */

int
rb_supports_ssl(void)
{
	return 1;
}

unsigned int
rb_ssl_handshake_count(rb_fde_t *const F)
{
	return F->handshake_count;
}

void
rb_ssl_clear_handshake_count(rb_fde_t *const F)
{
	F->handshake_count = 0;
}

void
rb_ssl_start_accepted(rb_fde_t *const F, ACCB *const cb, void *const data, const int timeout)
{
	F->type |= RB_FD_SSL;

	F->accept = rb_malloc(sizeof(struct acceptdata));
	F->accept->callback = cb;
	F->accept->data = data;
	F->accept->addrlen = 0;
	(void) memset(&F->accept->S, 0x00, sizeof F->accept->S);

	rb_settimeout(F, timeout, rb_ssl_timeout_cb, NULL);
	rb_ssl_init_fd(F, RB_FD_TLS_DIRECTION_IN);
	rb_ssl_accept_common(F, NULL);
}

void
rb_ssl_accept_setup(rb_fde_t *const srv_F, rb_fde_t *const cli_F, struct sockaddr *const st, const int addrlen)
{
	cli_F->type |= RB_FD_SSL;

	cli_F->accept = rb_malloc(sizeof(struct acceptdata));
	cli_F->accept->callback = srv_F->accept->callback;
	cli_F->accept->data = srv_F->accept->data;
	cli_F->accept->addrlen = (rb_socklen_t) addrlen;
	(void) memset(&cli_F->accept->S, 0x00, sizeof cli_F->accept->S);
	(void) memcpy(&cli_F->accept->S, st, (size_t) addrlen);

	rb_settimeout(cli_F, 10, rb_ssl_timeout_cb, NULL);
	rb_ssl_init_fd(cli_F, RB_FD_TLS_DIRECTION_IN);
	rb_ssl_accept_common(cli_F, NULL);
}

int
rb_ssl_listen(rb_fde_t *const F, const int backlog, const int defer_accept)
{
	int result = rb_listen(F, backlog, defer_accept);

	F->type = RB_FD_SOCKET | RB_FD_LISTEN | RB_FD_SSL;

	return result;
}

void
rb_connect_tcp_ssl(rb_fde_t *const F, struct sockaddr *const dest, struct sockaddr *const clocal,
                   CNCB *const callback, void *const data, const int timeout)
{
	if(F == NULL)
		return;

	struct ssl_connect *const sconn = rb_malloc(sizeof *sconn);
	sconn->data = data;
	sconn->callback = callback;
	sconn->timeout = timeout;

	rb_connect_tcp(F, dest, clocal, rb_ssl_tryconn, sconn, timeout);
}

void
rb_ssl_start_connected(rb_fde_t *const F, CNCB *const callback, void *const data, const int timeout)
{
	if(F == NULL)
		return;

	struct ssl_connect *const sconn = rb_malloc(sizeof *sconn);
	sconn->data = data;
	sconn->callback = callback;
	sconn->timeout = timeout;

	F->connect = rb_malloc(sizeof(struct conndata));
	F->connect->callback = callback;
	F->connect->data = data;

	F->type |= RB_FD_SSL;

	rb_settimeout(F, sconn->timeout, rb_ssl_tryconn_timeout_cb, sconn);
	rb_ssl_init_fd(F, RB_FD_TLS_DIRECTION_OUT);
	rb_ssl_connect_common(F, sconn);
}

#endif /* HAVE_GNUTLS */
