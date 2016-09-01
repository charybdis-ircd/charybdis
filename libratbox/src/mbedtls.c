/*
 *  libratbox: a library used by ircd-ratbox and other things
 *  mbedtls.c: ARM MbedTLS backend
 *
 *  Copyright (C) 2007-2008 ircd-ratbox development team
 *  Copyright (C) 2007-2008 Aaron Sethman <androsyn@ratbox.org>
 *  Copyright (C) 2015 William Pitcock <nenolod@dereferenced.org>
 *  Copyright (C) 2016 Aaron Jones <aaronmdjones@gmail.com>
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
 *  $Id$
 */

#include <libratbox_config.h>
#include <ratbox_lib.h>
#include <commio-int.h>
#include <commio-ssl.h>

#ifdef HAVE_MBEDTLS

#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/certs.h"
#include "mbedtls/x509.h"
#include "mbedtls/ssl.h"
#include "mbedtls/net.h"
#include "mbedtls/error.h"
#include "mbedtls/debug.h"
#include "mbedtls/dhm.h"
#include "mbedtls/version.h"

#include "mbedtls_embedded_data.h"

#define RB_MAX_CIPHERSUITES 512

typedef struct
{
	mbedtls_x509_crt	 crt;
	mbedtls_pk_context	 key;
	mbedtls_dhm_context	 dhp;
	mbedtls_ssl_config	 server_cfg;
	mbedtls_ssl_config	 client_cfg;
	int			 suites[RB_MAX_CIPHERSUITES + 1];
	size_t			 refcount;
} rb_mbedtls_cfg_context;

typedef struct
{
	rb_mbedtls_cfg_context	*cfg;
	mbedtls_ssl_context	 ssl;
} rb_mbedtls_ssl_context;

#define SSL_C(x)  ((rb_mbedtls_ssl_context *) (x)->ssl)->cfg
#define SSL_P(x) &((rb_mbedtls_ssl_context *) (x)->ssl)->ssl

static mbedtls_ctr_drbg_context ctr_drbg_ctx;
static mbedtls_entropy_context entropy_ctx;

static mbedtls_x509_crt dummy_ca_ctx;
static rb_mbedtls_cfg_context *rb_mbedtls_cfg = NULL;



struct ssl_connect
{
	CNCB *callback;
	void *data;
	int timeout;
};

static void rb_ssl_connect_realcb(rb_fde_t *, int, struct ssl_connect *);

static int rb_sock_net_recv(void *, unsigned char *, size_t);
static int rb_sock_net_xmit(void *, const unsigned char *, size_t);



/*
 * Internal MbedTLS-specific code
 */

static const char *
rb_get_ssl_strerror_internal(int err)
{
	static char errbuf[512];

#ifdef MBEDTLS_ERROR_C
	char mbed_errbuf[512];
	mbedtls_strerror(err, mbed_errbuf, sizeof mbed_errbuf);
	(void) rb_snprintf(errbuf, sizeof errbuf, "(-0x%x) %s", -err, mbed_errbuf);
#else
	(void) rb_snprintf(errbuf, sizeof errbuf, "-0x%x", -err);
#endif

	return errbuf;
}

static void
rb_mbedtls_cfg_incref(rb_mbedtls_cfg_context *const cfg)
{
	lrb_assert(cfg->refcount > 0);

	cfg->refcount++;
}

static void
rb_mbedtls_cfg_decref(rb_mbedtls_cfg_context *const cfg)
{
	if(cfg == NULL)
		return;

	lrb_assert(cfg->refcount > 0);

	if((--cfg->refcount) > 0)
		return;

	mbedtls_ssl_config_free(&cfg->client_cfg);
	mbedtls_ssl_config_free(&cfg->server_cfg);
	mbedtls_dhm_free(&cfg->dhp);
	mbedtls_pk_free(&cfg->key);
	mbedtls_x509_crt_free(&cfg->crt);

	rb_free(cfg);
}

static rb_mbedtls_cfg_context *
rb_mbedtls_cfg_new(void)
{
	rb_mbedtls_cfg_context *const cfg = rb_malloc(sizeof *cfg);

	if(cfg == NULL)
		return NULL;

	mbedtls_x509_crt_init(&cfg->crt);
	mbedtls_pk_init(&cfg->key);
	mbedtls_dhm_init(&cfg->dhp);
	mbedtls_ssl_config_init(&cfg->server_cfg);
	mbedtls_ssl_config_init(&cfg->client_cfg);

	(void) memset(cfg->suites, 0x00, sizeof cfg->suites);

	cfg->refcount = 1;

	int ret;

	if((ret = mbedtls_ssl_config_defaults(&cfg->server_cfg,
	             MBEDTLS_SSL_IS_SERVER, MBEDTLS_SSL_TRANSPORT_STREAM,
	             MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
	{
		rb_lib_log("rb_mbedtls_cfg_new: ssl_config_defaults (server): %s",
		           rb_get_ssl_strerror_internal(ret));

		rb_mbedtls_cfg_decref(cfg);
		return NULL;
	}

	if((ret = mbedtls_ssl_config_defaults(&cfg->client_cfg,
	             MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM,
	             MBEDTLS_SSL_PRESET_DEFAULT)) != 0)
	{
		rb_lib_log("rb_mbedtls_cfg_new: ssl_config_defaults (client): %s",
		           rb_get_ssl_strerror_internal(ret));

		rb_mbedtls_cfg_decref(cfg);
		return NULL;
	}

	mbedtls_ssl_conf_rng(&cfg->server_cfg, mbedtls_ctr_drbg_random, &ctr_drbg_ctx);
	mbedtls_ssl_conf_rng(&cfg->client_cfg, mbedtls_ctr_drbg_random, &ctr_drbg_ctx);

	mbedtls_ssl_conf_ca_chain(&cfg->server_cfg, &dummy_ca_ctx, NULL);
	mbedtls_ssl_conf_ca_chain(&cfg->client_cfg, &dummy_ca_ctx, NULL);

	mbedtls_ssl_conf_authmode(&cfg->server_cfg, MBEDTLS_SSL_VERIFY_OPTIONAL);
	mbedtls_ssl_conf_authmode(&cfg->client_cfg, MBEDTLS_SSL_VERIFY_NONE);

	#ifdef MBEDTLS_SSL_LEGACY_BREAK_HANDSHAKE
	mbedtls_ssl_conf_legacy_renegotiation(&cfg->client_cfg, MBEDTLS_SSL_LEGACY_BREAK_HANDSHAKE);
	#endif

	#ifdef MBEDTLS_SSL_SESSION_TICKETS_DISABLED
	mbedtls_ssl_conf_session_tickets(&cfg->client_cfg, MBEDTLS_SSL_SESSION_TICKETS_DISABLED);
	#endif

	return cfg;
}

static void
rb_ssl_setup_mbed_context(rb_fde_t *const F, mbedtls_ssl_config *const mbed_config)
{
	rb_mbedtls_ssl_context *const mbed_ssl_ctx = rb_malloc(sizeof *mbed_ssl_ctx);
	if(mbed_ssl_ctx == NULL)
	{
		rb_lib_log("rb_ssl_setup_mbed_context: rb_malloc: allocation failure");
		rb_close(F);
		return;
	}

	mbedtls_ssl_init(&mbed_ssl_ctx->ssl);
	mbedtls_ssl_set_bio(&mbed_ssl_ctx->ssl, F, rb_sock_net_xmit, rb_sock_net_recv, NULL);

	int ret;

	if((ret = mbedtls_ssl_setup(&mbed_ssl_ctx->ssl, mbed_config)) != 0)
	{
		rb_lib_log("rb_ssl_setup_mbed_context: ssl_setup: %s",
		           rb_get_ssl_strerror_internal(ret));
		mbedtls_ssl_free(&mbed_ssl_ctx->ssl);
		rb_free(mbed_ssl_ctx);
		rb_close(F);
		return;
	}

	rb_mbedtls_cfg_incref(rb_mbedtls_cfg);
	mbed_ssl_ctx->cfg = rb_mbedtls_cfg;
	F->ssl = mbed_ssl_ctx;
}

static void
rb_ssl_accept_common(rb_fde_t *const F, void *const data)
{
	lrb_assert(F != NULL);
	lrb_assert(F->accept != NULL);
	lrb_assert(F->accept->callback != NULL);
	lrb_assert(F->ssl != NULL);

	mbedtls_ssl_context *const ssl_ctx = SSL_P(F);

	if(ssl_ctx->state != MBEDTLS_SSL_HANDSHAKE_OVER)
	{
		int ret = mbedtls_ssl_handshake(ssl_ctx);

		switch(ret)
		{
		case 0:
			F->handshake_count++;
			break;
		case MBEDTLS_ERR_SSL_WANT_READ:
			rb_setselect(F, RB_SELECT_READ, rb_ssl_accept_common, NULL);
			return;
		case MBEDTLS_ERR_SSL_WANT_WRITE:
			rb_setselect(F, RB_SELECT_WRITE, rb_ssl_accept_common, NULL);
			return;
		default:
			F->ssl_errno = ret;
			F->accept->callback(F, RB_ERROR_SSL, NULL, 0, F->accept->data);
			return;
		}
	}

	rb_settimeout(F, 0, NULL, NULL);
	rb_setselect(F, RB_SELECT_READ | RB_SELECT_WRITE, NULL, NULL);

	struct acceptdata *const ad = F->accept;
	F->accept = NULL;
	ad->callback(F, RB_OK, (struct sockaddr *)&ad->S, ad->addrlen, ad->data);
	rb_free(ad);
}

static void
rb_ssl_tryconn_cb(rb_fde_t *const F, void *const data)
{
	lrb_assert(F != NULL);
	lrb_assert(F->ssl != NULL);

	mbedtls_ssl_context *const ssl_ctx = SSL_P(F);

	if(ssl_ctx->state != MBEDTLS_SSL_HANDSHAKE_OVER)
	{
		int ret = mbedtls_ssl_handshake(ssl_ctx);

		switch(ret)
		{
		case 0:
			F->handshake_count++;
			break;
		case MBEDTLS_ERR_SSL_WANT_READ:
			rb_setselect(F, RB_SELECT_READ, rb_ssl_tryconn_cb, data);
			return;
		case MBEDTLS_ERR_SSL_WANT_WRITE:
			rb_setselect(F, RB_SELECT_WRITE, rb_ssl_tryconn_cb, data);
			return;
		default:
			errno = EIO;
			F->ssl_errno = ret;
			rb_ssl_connect_realcb(F, RB_ERROR_SSL, data);
			return;
		}
	}

	rb_ssl_connect_realcb(F, RB_OK, data);
}



/*
 * External MbedTLS-specific code
 */

void
rb_ssl_shutdown(rb_fde_t *const F)
{
	if(F == NULL || F->ssl == NULL)
		return;

	if(SSL_P(F) != NULL)
	{
		for(int i = 0; i < 4; i++)
		{
			int ret = mbedtls_ssl_close_notify(SSL_P(F));

			if(ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE)
				break;
		}
		mbedtls_ssl_free(SSL_P(F));
	}

	if(SSL_C(F) != NULL)
		rb_mbedtls_cfg_decref(SSL_C(F));

	rb_free(F->ssl);
	F->ssl = NULL;
}

int
rb_init_ssl(void)
{
	mbedtls_ctr_drbg_init(&ctr_drbg_ctx);
	mbedtls_entropy_init(&entropy_ctx);

	int ret;

	if((ret = mbedtls_ctr_drbg_seed(&ctr_drbg_ctx, mbedtls_entropy_func, &entropy_ctx,
	    (const unsigned char *)rb_mbedtls_personal_str, sizeof(rb_mbedtls_personal_str))) != 0)
	{
		rb_lib_log("rb_init_ssl: ctr_drbg_seed: %s",
		           rb_get_ssl_strerror_internal(ret));
		return 0;
	}

	if((ret = mbedtls_x509_crt_parse_der(&dummy_ca_ctx, rb_mbedtls_dummy_ca_certificate,
	                                     sizeof(rb_mbedtls_dummy_ca_certificate))) != 0)
	{
		rb_lib_log("rb_init_ssl: x509_crt_parse_der (Dummy CA): %s",
		           rb_get_ssl_strerror_internal(ret));
		return 0;
	}

	rb_lib_log("rb_init_ssl: MbedTLS backend initialised");
	return 1;
}

int
rb_setup_ssl_server(const char *const certfile, const char *keyfile,
                    const char *const dhfile, const char *const cipherlist)
{
	if(certfile == NULL)
	{
		rb_lib_log("rb_setup_ssl_server: no certificate file specified");
		return 0;
	}

	if(keyfile == NULL)
		keyfile = certfile;

	rb_mbedtls_cfg_context *const newcfg = rb_mbedtls_cfg_new();

	if(newcfg == NULL)
	{
		rb_lib_log("rb_setup_ssl_server: rb_mbedtls_cfg_new: allocation failed");
		return 0;
	}

	int ret;

	if((ret = mbedtls_x509_crt_parse_file(&newcfg->crt, certfile)) != 0)
	{
		rb_lib_log("rb_setup_ssl_server: x509_crt_parse_file ('%s'): %s",
		           certfile, rb_get_ssl_strerror_internal(ret));

		rb_mbedtls_cfg_decref(newcfg);
		return 0;
	}
	if((ret = mbedtls_pk_parse_keyfile(&newcfg->key, keyfile, NULL)) != 0)
	{
		rb_lib_log("rb_setup_ssl_server: pk_parse_keyfile ('%s'): %s",
		           keyfile, rb_get_ssl_strerror_internal(ret));

		rb_mbedtls_cfg_decref(newcfg);
		return 0;
	}

	/* Absense of DH parameters does not matter with mbedTLS, as it comes with its own defaults
	   Thus, clients can still use DHE- ciphersuites, just over a weaker, common DH group
	   So, we do not consider failure to parse DH parameters as fatal */
	if(dhfile == NULL)
	{
		rb_lib_log("rb_setup_ssl_server: no DH parameters file specified");
	}
	else
	{
		if((ret = mbedtls_dhm_parse_dhmfile(&newcfg->dhp, dhfile)) != 0)
		{
			rb_lib_log("rb_setup_ssl_server: dhm_parse_dhmfile ('%s'): %s",
			           dhfile, rb_get_ssl_strerror_internal(ret));
		}
		else if((ret = mbedtls_ssl_conf_dh_param_ctx(&newcfg->server_cfg, &newcfg->dhp)) != 0)
		{
			rb_lib_log("rb_setup_ssl_server: ssl_conf_dh_param_ctx: %s",
			           rb_get_ssl_strerror_internal(ret));
		}
	}

	if((ret = mbedtls_ssl_conf_own_cert(&newcfg->server_cfg, &newcfg->crt, &newcfg->key)) != 0)
	{
		rb_lib_log("rb_setup_ssl_server: ssl_conf_own_cert (server): %s",
		           rb_get_ssl_strerror_internal(ret));

		rb_mbedtls_cfg_decref(newcfg);
		return 0;
	}
	if((ret = mbedtls_ssl_conf_own_cert(&newcfg->client_cfg, &newcfg->crt, &newcfg->key)) != 0)
	{
		rb_lib_log("rb_setup_ssl_server: ssl_conf_own_cert (client): %s",
		           rb_get_ssl_strerror_internal(ret));

		rb_mbedtls_cfg_decref(newcfg);
		return 0;
	}

	if(cipherlist != NULL)
	{
		// The cipherlist is (const char *) -- we should not modify it
		char *const cipherlist_dup = strdup(cipherlist);

		if(cipherlist_dup == NULL)
		{
			rb_lib_log("rb_setup_ssl_server: strdup: %s", strerror(errno));
			rb_lib_log("rb_setup_ssl_server: will not configure ciphersuites!");
		}
		else
		{
			size_t suites_count = 0;
			char *cipher_str = cipherlist_dup;
			char *cipher_idx;

			do
			{
				// Arbitrary, but the same separator as OpenSSL uses
				cipher_idx = strchr(cipher_str, ':');

				// This could legitimately be NULL (last ciphersuite in the list)
				if(cipher_idx != NULL)
					*cipher_idx = '\0';

				size_t cipher_len = strlen(cipher_str);
				int cipher_idn = 0;

				// All MbedTLS ciphersuite names begin with these 4 characters
				if(cipher_len > 4 && strncmp(cipher_str, "TLS-", 4) == 0)
					cipher_idn = mbedtls_ssl_get_ciphersuite_id(cipher_str);

				// Prevent the same ciphersuite being added multiple times
				for(size_t x = 0; cipher_idn != 0 && newcfg->suites[x] != 0; x++)
					if(newcfg->suites[x] == cipher_idn)
						cipher_idn = 0;

				// Add the suite to the list
				if(cipher_idn != 0)
					newcfg->suites[suites_count++] = cipher_idn;

				// Advance the string to the next entry
				if (cipher_idx)
					cipher_str = cipher_idx + 1;

			} while(cipher_idx && suites_count < RB_MAX_CIPHERSUITES);

			if(suites_count > 0)
			{
				mbedtls_ssl_conf_ciphersuites(&newcfg->server_cfg, newcfg->suites);
				mbedtls_ssl_conf_ciphersuites(&newcfg->client_cfg, newcfg->suites);
				rb_lib_log("rb_setup_ssl_server: Configured %zu ciphersuites", suites_count);
			}
			else
				rb_lib_log("rb_setup_ssl_server: Passed a list of ciphersuites, but could not configure any");

			free(cipherlist_dup);
		}
	}

	rb_mbedtls_cfg_decref(rb_mbedtls_cfg);
	rb_mbedtls_cfg = newcfg;

	rb_lib_log("rb_setup_ssl_server: TLS configuration successful");
	return 1;
}

int
rb_init_prng(const char *const path, prng_seed_t seed_type)
{
	rb_lib_log("rb_init_prng: Skipping PRNG initialisation; not required by MbedTLS backend");
	return 1;
}

int
rb_get_random(void *const buf, size_t length)
{
	int ret;

	if((ret = mbedtls_ctr_drbg_random(&ctr_drbg_ctx, buf, length)) != 0)
	{
		rb_lib_log("rb_get_random: ctr_drbg_random: %s",
		           rb_get_ssl_strerror_internal(ret));

		return 0;
	}

	return 1;
}

const char *
rb_get_ssl_strerror(rb_fde_t *const F)
{
	return rb_get_ssl_strerror_internal(F->ssl_errno);
}

int
rb_get_ssl_certfp(rb_fde_t *const F, uint8_t certfp[const RB_SSL_CERTFP_LEN], int method)
{
	mbedtls_md_type_t md_type;
	int hashlen;

	switch(method)
	{
	case RB_SSL_CERTFP_METH_SHA1:
		md_type = MBEDTLS_MD_SHA1;
		hashlen = RB_SSL_CERTFP_LEN_SHA1;
		break;
	case RB_SSL_CERTFP_METH_SHA256:
		md_type = MBEDTLS_MD_SHA256;
		hashlen = RB_SSL_CERTFP_LEN_SHA256;
		break;
	case RB_SSL_CERTFP_METH_SHA512:
		md_type = MBEDTLS_MD_SHA512;
		hashlen = RB_SSL_CERTFP_LEN_SHA512;
		break;
	default:
		return 0;
	}

	const mbedtls_x509_crt *const peer_cert = mbedtls_ssl_get_peer_cert(SSL_P(F));

	if(peer_cert == NULL)
		return 0;

	const mbedtls_md_info_t *const md_info = mbedtls_md_info_from_type(md_type);

	if(md_info == NULL)
		return 0;

	int ret;

	if((ret = mbedtls_md(md_info, peer_cert->raw.p, peer_cert->raw.len, certfp)) != 0)
	{
		rb_lib_log("rb_get_ssl_certfp: mbedtls_md: %s",
		           rb_get_ssl_strerror_internal(ret));
		return 0;
	}

	return hashlen;
}

void
rb_get_ssl_info(char *const buf, size_t len)
{
	char version_str[512];

	mbedtls_version_get_string(version_str);

	(void) rb_snprintf(buf, len, "ARM mbedTLS: compiled (v%s), library (v%s)",
	                   MBEDTLS_VERSION_STRING, version_str);
}

const char *
rb_ssl_get_cipher(rb_fde_t *const F)
{
	if(F == NULL || F->ssl == NULL || SSL_P(F) == NULL)
		return NULL;

	static char buf[512];

	const char *const version = mbedtls_ssl_get_version(SSL_P(F));
	const char *const cipher = mbedtls_ssl_get_ciphersuite(SSL_P(F));

	(void) rb_snprintf(buf, sizeof buf, "%s, %s", version, cipher);

	return buf;
}

ssize_t
rb_ssl_read(rb_fde_t *const F, void *const buf, size_t count)
{
	lrb_assert(F != NULL);
	lrb_assert(F->ssl != NULL);

	ssize_t ret = (ssize_t) mbedtls_ssl_read(SSL_P(F), buf, count);

	if(ret > 0)
		return ret;

	switch(ret)
	{
	case MBEDTLS_ERR_SSL_WANT_READ:
		errno = EAGAIN;
		return RB_RW_SSL_NEED_READ;
	case MBEDTLS_ERR_SSL_WANT_WRITE:
		errno = EAGAIN;
		return RB_RW_SSL_NEED_WRITE;
	default:
		errno = EIO;
		F->ssl_errno = ret;
		return RB_RW_SSL_ERROR;
	}
}

ssize_t
rb_ssl_write(rb_fde_t *const F, const void *const buf, size_t count)
{
	lrb_assert(F != NULL);
	lrb_assert(F->ssl != NULL);

	ssize_t ret = (ssize_t) mbedtls_ssl_write(SSL_P(F), buf, count);

	if(ret > 0)
		return ret;

	switch(ret)
	{
	case MBEDTLS_ERR_SSL_WANT_READ:
		errno = EAGAIN;
		return RB_RW_SSL_NEED_READ;
	case MBEDTLS_ERR_SSL_WANT_WRITE:
		errno = EAGAIN;
		return RB_RW_SSL_NEED_WRITE;
	default:
		errno = EIO;
		F->ssl_errno = ret;
		return RB_RW_SSL_ERROR;
	}
}



/*
 * Internal library-agnostic code
 * Mostly copied from the OpenSSL backend, with some optimisations and complete const-correctness
 */

static void
rb_ssl_connect_realcb(rb_fde_t *const F, int status, struct ssl_connect *const sconn)
{
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
rb_ssl_tryconn(rb_fde_t *const F, int status, void *const data)
{
	lrb_assert(F != NULL);

	if(status != RB_OK)
	{
		rb_ssl_connect_realcb(F, status, data);
		return;
	}

	F->type |= RB_FD_SSL;

	struct ssl_connect *const sconn = data;
	rb_settimeout(F, sconn->timeout, rb_ssl_tryconn_timeout_cb, sconn);

	rb_ssl_setup_mbed_context(F, &rb_mbedtls_cfg->client_cfg);
	rb_ssl_tryconn_cb(F, sconn);
}

static int
rb_sock_net_recv(void *const context_ptr, unsigned char *const buf, size_t count)
{
	rb_fde_t *const F = (rb_fde_t *)context_ptr;

	int ret = (int) read(F->fd, buf, count);

	if(ret < 0 && rb_ignore_errno(errno))
		return MBEDTLS_ERR_SSL_WANT_READ;

	return ret;
}

static int
rb_sock_net_xmit(void *const context_ptr, const unsigned char *const buf, size_t count)
{
	rb_fde_t *const F = (rb_fde_t *)context_ptr;

	int ret = (int) write(F->fd, buf, count);

	if(ret < 0 && rb_ignore_errno(errno))
		return MBEDTLS_ERR_SSL_WANT_READ;

	return ret;
}



/*
 * External library-agnostic code
 * Mostly copied from the OpenSSL backend, with some optimisations and const-correctness
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
rb_ssl_start_accepted(rb_fde_t *const F, ACCB *const cb, void *const data, int timeout)
{
	F->type |= RB_FD_SSL;
	F->accept = rb_malloc(sizeof(struct acceptdata));

	F->accept->callback = cb;
	F->accept->data = data;
	rb_settimeout(F, timeout, rb_ssl_timeout_cb, NULL);

	F->accept->addrlen = 0;
	(void) memset(&F->accept->S, 0x00, sizeof F->accept->S);

	rb_ssl_setup_mbed_context(F, &rb_mbedtls_cfg->server_cfg);
	rb_ssl_accept_common(F, NULL);
}

void
rb_ssl_accept_setup(rb_fde_t *const srv_F, rb_fde_t *const cli_F, struct sockaddr *const st, int addrlen)
{
	cli_F->type |= RB_FD_SSL;
	cli_F->accept = rb_malloc(sizeof(struct acceptdata));

	cli_F->accept->callback = srv_F->accept->callback;
	cli_F->accept->data = srv_F->accept->data;
	rb_settimeout(cli_F, 10, rb_ssl_timeout_cb, NULL);

	cli_F->accept->addrlen = addrlen;
	(void) memset(&cli_F->accept->S, 0x00, sizeof cli_F->accept->S);
	(void) memcpy(&cli_F->accept->S, st, addrlen);

	rb_ssl_setup_mbed_context(cli_F, &rb_mbedtls_cfg->server_cfg);
	rb_ssl_accept_common(cli_F, NULL);
}

int
rb_ssl_listen(rb_fde_t *const F, int backlog, int defer_accept)
{
	int result = rb_listen(F, backlog, defer_accept);
	F->type = RB_FD_SOCKET | RB_FD_LISTEN | RB_FD_SSL;

	return result;
}

void
rb_connect_tcp_ssl(rb_fde_t *const F, struct sockaddr *const dest, struct sockaddr *const clocal, int socklen,
                   CNCB *const callback, void *const data, int timeout)
{
	if(F == NULL)
		return;

	struct ssl_connect *const sconn = rb_malloc(sizeof *sconn);
	sconn->data = data;
	sconn->callback = callback;
	sconn->timeout = timeout;
	rb_connect_tcp(F, dest, clocal, socklen, rb_ssl_tryconn, sconn, timeout);
}

void
rb_ssl_start_connected(rb_fde_t *const F, CNCB *const callback, void *const data, int timeout)
{
	if(F == NULL)
		return;

	F->connect = rb_malloc(sizeof(struct conndata));
	F->connect->callback = callback;
	F->connect->data = data;
	F->type |= RB_FD_SSL;

	struct ssl_connect *const sconn = rb_malloc(sizeof *sconn);
	sconn->data = data;
	sconn->callback = callback;
	sconn->timeout = timeout;

	rb_settimeout(F, sconn->timeout, rb_ssl_tryconn_timeout_cb, sconn);

	rb_ssl_setup_mbed_context(F, &rb_mbedtls_cfg->client_cfg);
	rb_ssl_tryconn_cb(F, sconn);
}

#endif /* HAVE_MBEDTLS */
