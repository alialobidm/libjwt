/* Copyright (C) 2024 Ben Collins <bcollins@maclara-llc.com>
   This file is part of the JWT C Library

   SPDX-License-Identifier:  MPL-2.0
   This Source Code Form is subject to the terms of the Mozilla Public
   License, v. 2.0. If a copy of the MPL was not distributed with this
   file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* TODO: Use LibJWT for most of this now that it has the functionality. */

/* XXX BIG FAT WARNING: There's not much error checking here. */

/* XXX: Also, requires OpenSSL v3. I wont accept patches for lower versions. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <openssl/pem.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/core_names.h>
#include <openssl/err.h>
#include <openssl/param_build.h>

#include <jansson.h>

#include <jwt.h>
#include <jwt-private.h>

#ifndef EVP_PKEY_PRIVATE_KEY
#define EVP_PKEY_PRIVATE_KEY EVP_PKEY_KEYPAIR
#endif

static int ec_count, rsa_count, eddsa_count, rsa_pss_count;

static void print_openssl_errors_and_exit(const char *func, const int line)
{
	fprintf(stderr, "Error at %s:%d\n", func, line);
	ERR_print_errors_fp(stderr);
	exit(EXIT_FAILURE);
}

#define jwt_exit() print_openssl_errors_and_exit(__func__, __LINE__)

#define trace() fprintf(stderr, "%s:%d\n", __func__, __LINE__)

/* Sets a param for the public EC key */
static void set_ec_pub_key(OSSL_PARAM_BLD *build, json_t *jx, json_t *jy,
			   const char *curve_name)
{
	EC_GROUP *group = NULL;
	EC_POINT *point = NULL;
	unsigned char *bin_x, *bin_y;
	int len_x, len_y;
	const char *str_x, *str_y;
	BIGNUM *x = NULL, *y = NULL;
	int nid;
	size_t pub_key_len = 0;
	unsigned char *pub_key = NULL;

	/* First, base64url decode */
	str_x = json_string_value(jx);
	str_y = json_string_value(jy);
	bin_x = jwt_base64uri_decode(str_x, &len_x);
	bin_y = jwt_base64uri_decode(str_y, &len_y);
	if (bin_x == NULL || bin_y == NULL)
		jwt_exit();

	/* Convert to BN */
	x = BN_bin2bn(bin_x, len_x, NULL);
	y = BN_bin2bn(bin_y, len_y, NULL);
	if (x == NULL || y == NULL)
		jwt_exit();
	jwt_freemem(bin_x);
	jwt_freemem(bin_y);

	/* Create the EC group and point */
	nid = OBJ_sn2nid(curve_name);
	group = EC_GROUP_new_by_curve_name(nid);
	point = EC_POINT_new(group);
	EC_POINT_set_affine_coordinates(group, point, x, y, NULL);
	pub_key_len = EC_POINT_point2buf(group, point, POINT_CONVERSION_UNCOMPRESSED, &pub_key, NULL);

	EC_GROUP_free(group);
	EC_POINT_free(point);
	BN_free(x);
	BN_free(y);

	OSSL_PARAM_BLD_push_octet_string(build, OSSL_PKEY_PARAM_PUB_KEY, pub_key, pub_key_len);
	// XXX OPENSSL_free(pub_key);
}

/* b64url-decodes a single OSSL BIGNUM and sets the OSSL param. */
static void set_one_bn(OSSL_PARAM_BLD *build, const char *ossl_name,
		       json_t *val)
{
	unsigned char *bin;
	const char *str;
	int len = 0;
	BIGNUM *bn;

	/* decode it */
	str = json_string_value(val);
	bin = jwt_base64uri_decode(str, &len);

	if (bin == NULL || len <= 0)
		jwt_exit();

	bn = BN_bin2bn(bin, len, NULL);
	jwt_freemem(bin);

	OSSL_PARAM_BLD_push_BN(build, ossl_name, bn);
	// XXX BN_free(bn);
}

/* Sets a single OSSL string param. */
static void set_one_string(OSSL_PARAM_BLD *build, const char *ossl_name,
			   json_t *val)
{
	const char *str = json_string_value(val);
	int len = json_string_length(val);

	OSSL_PARAM_BLD_push_utf8_string(build, ossl_name, str, len);
}

/* b64url-decodes a single octet and creates an OSSL param. */
static void set_one_octet(OSSL_PARAM_BLD *build, const char *ossl_name,
			  json_t *val)
{
	unsigned char *bin;
	const char *str;
	int len;

	/* decode it */
	str = json_string_value(val);
	bin = jwt_base64uri_decode(str, &len);

	OSSL_PARAM_BLD_push_octet_string(build, ossl_name, bin, len);
	// XXX jwt_freemem(bin);
}

static void write_key_file(EVP_PKEY *pkey, const char *pre, const char *name,
			   json_t *kid, int priv)
{
	char *file_name;
	FILE *fp;
	int ret;

	if (kid == NULL) {
		ret = asprintf(&file_name, "pems/%s-%s%s.pem", pre, name,
			       priv ? "" : "-pub");
	} else {
		ret = asprintf(&file_name, "pems/%s-%s_%s%s.pem", pre,
			       name, json_string_value(kid),
			       priv ? "" : "_pub");
	}

	if (ret < 0) {
		fprintf(stderr, "Memory error writing file\n");
		return;
	}

	fp = fopen(file_name, "wx");
	if (fp == NULL) {
		fprintf(stderr, "Could not overwrite '%s'\n", file_name);
		free(file_name);
		return;
	}

	if (priv)
		PEM_write_PrivateKey(fp, pkey, NULL, NULL, 0, NULL, NULL);
	else
		PEM_write_PUBKEY(fp, pkey);

	free(file_name);
}

/* For EdDSA keys (EDDSA) */
static void __process_eddsa_jwk(json_t *jwk)
{
	OSSL_PARAM *params;
	OSSL_PARAM_BLD *build;
	EVP_PKEY_CTX *pctx = NULL;
	EVP_PKEY *pkey = NULL;
	json_auto_t *x, *d, *kid;
	int priv = 0;

	x = json_object_get(jwk, "x");
	d = json_object_get(jwk, "d");
	kid = json_object_get(jwk, "kid");

	if (x == NULL) {
		fprintf(stderr, "Invalid EdDSA key\n");
		return;
	}

	if (d != NULL)
		priv = 1;
	
	pctx = EVP_PKEY_CTX_new_from_name(NULL, "ED25519", NULL);
	if (pctx == NULL)
		jwt_exit();

	if (EVP_PKEY_fromdata_init(pctx) <= 0)
		jwt_exit();

	build = OSSL_PARAM_BLD_new();

	set_one_octet(build, OSSL_PKEY_PARAM_PUB_KEY, x);
	if (priv)
		set_one_octet(build, OSSL_PKEY_PARAM_PRIV_KEY, d);

	params = OSSL_PARAM_BLD_to_param(build);

	/* Create EVP_PKEY from params */
	if (priv)
		EVP_PKEY_fromdata(pctx, &pkey, EVP_PKEY_PRIVATE_KEY, params);
	else
		EVP_PKEY_fromdata(pctx, &pkey, EVP_PKEY_PUBLIC_KEY, params);

	OSSL_PARAM_BLD_free(build);

	if (pkey == NULL)
		jwt_exit();

	write_key_file(pkey, "eddsa", "ED25519", kid, priv);

	eddsa_count++;
}

/* For RSA keys (RS256, RS384, RS512). Also works for RSA-PSS
 * (PS256, PS384, PS512) */
static void __process_rsa_jwk(json_t *jwk)
{
	OSSL_PARAM_BLD *build;
	json_auto_t *n, *e, *d, *p, *q, *dp, *dq, *qi, *kid, *alg;
	int is_rsa_pss = 0, priv = 0;
	OSSL_PARAM *params;
	EVP_PKEY *pkey = NULL;
	EVP_PKEY_CTX *pctx = NULL;
	const char *alg_str = NULL;
	char bits_str[32];
	int bits;

	alg = json_object_get(jwk, "alg");
	n = json_object_get(jwk, "n");
	e = json_object_get(jwk, "e");
	d = json_object_get(jwk, "d");
	p = json_object_get(jwk, "p");
	q = json_object_get(jwk, "q");
	dp = json_object_get(jwk, "dp");
	dq = json_object_get(jwk, "dq");
	qi = json_object_get(jwk, "qi");
	kid = json_object_get(jwk, "kid");

	if (n == NULL || e == NULL) {
		fprintf(stderr, "Invalid RSA key\n");
		return;
	}

	/* Check alg to see if we can sniff RSA vs RSA-PSS */
	if (alg) {
		alg_str = json_string_value(alg);

		if (alg_str[0] == 'P')
			is_rsa_pss = 1;
	}

	/* Priv vs PUB */
	if (d != NULL) {
		if (!p || !q || !dp || !dq || !qi) {
			fprintf(stderr, "Invalid private RSA key\n");
			return;
		}
		priv = 1;
	}

	pctx = EVP_PKEY_CTX_new_from_name(NULL, is_rsa_pss ? "RSA-PSS" : "RSA",
					  NULL);
	if (pctx == NULL)
		jwt_exit();

	if (EVP_PKEY_fromdata_init(pctx) <= 0)
		jwt_exit();

	/* Set params */
	build = OSSL_PARAM_BLD_new();

	set_one_bn(build, OSSL_PKEY_PARAM_RSA_N, n);
	set_one_bn(build, OSSL_PKEY_PARAM_RSA_E, e);

	if (priv) {
		set_one_bn(build, OSSL_PKEY_PARAM_RSA_D, d);
		set_one_bn(build, OSSL_PKEY_PARAM_RSA_FACTOR1, p);
		set_one_bn(build, OSSL_PKEY_PARAM_RSA_FACTOR2, q);
		set_one_bn(build, OSSL_PKEY_PARAM_RSA_EXPONENT1, dp);
		set_one_bn(build, OSSL_PKEY_PARAM_RSA_EXPONENT2, dq);
		set_one_bn(build, OSSL_PKEY_PARAM_RSA_COEFFICIENT1, qi);
	}

	params = OSSL_PARAM_BLD_to_param(build);

	/* Create EVP_PKEY from params */
	if (priv)
		EVP_PKEY_fromdata(pctx, &pkey, EVP_PKEY_PRIVATE_KEY, params);
	else
		EVP_PKEY_fromdata(pctx, &pkey, EVP_PKEY_PUBLIC_KEY, params);

	OSSL_PARAM_BLD_free(build);

	if (pkey == NULL)
		jwt_exit();

	/* XXX This doesn't work if, for example, we load a RSA-PSS key as
	 * a RSA key. It doesn't break things for the PEM creation, but
	 * it does mislabel thing. */
	if (EVP_PKEY_get_id(pkey) == EVP_PKEY_RSA_PSS)
		is_rsa_pss = 1;

	EVP_PKEY_get_int_param(pkey, OSSL_PKEY_PARAM_BITS, &bits);
	sprintf(bits_str, "%d", bits);

	write_key_file(pkey, is_rsa_pss ? "rsa-pss" : "rsa", bits_str,
		       kid, priv);

	if (is_rsa_pss)
		rsa_pss_count++;
	else
		rsa_count++;
}

/* For EC Keys (ES256, ES384, ES512) */
static void __process_ec_jwk(json_t *jwk)
{
	OSSL_PARAM *params;
	OSSL_PARAM_BLD *build;
	json_auto_t *crv, *x, *y, *d, *kid;
	EVP_PKEY *pkey = NULL;
	EVP_PKEY_CTX *pctx = NULL;
	const char *crv_str;
	int priv = 0;

	crv = json_object_get(jwk, "crv");
	x = json_object_get(jwk, "x");
	y = json_object_get(jwk, "y");
	d = json_object_get(jwk, "d");
	kid = json_object_get(jwk, "kid");

	/* Check the minimal for pub key */
	if (crv == NULL || x == NULL || y == NULL) {
		fprintf(stderr, "Invalid EC key\n");
		return;
	}

	crv_str = json_string_value(crv);

	/* Only private keys contain this field */
	if (d != NULL)
		priv = 1;

	pctx = EVP_PKEY_CTX_new_from_name(NULL, "EC", NULL);
	if (pctx == NULL)
		jwt_exit();

	if (EVP_PKEY_fromdata_init(pctx) <= 0)
		jwt_exit();

	/* Set params */
	build = OSSL_PARAM_BLD_new();

	set_one_string(build, OSSL_PKEY_PARAM_GROUP_NAME, crv);
	set_ec_pub_key(build, x, y, crv_str);

	if (priv)
		set_one_bn(build, OSSL_PKEY_PARAM_PRIV_KEY, d);

	params = OSSL_PARAM_BLD_to_param(build);

	/* Create EVP_PKEY from params */
	if (priv)
		EVP_PKEY_fromdata(pctx, &pkey, EVP_PKEY_PRIVATE_KEY, params);
	else
		EVP_PKEY_fromdata(pctx, &pkey, EVP_PKEY_PUBLIC_KEY, params);

	OSSL_PARAM_BLD_free(build);

	if (pkey == NULL)
		jwt_exit();

	write_key_file(pkey, "ec", crv_str, kid, priv);

	ec_count++;
}

static void __process_one_jwk(json_t *jwk)
{
	static int count = 1;
	const char *kty;
	json_auto_t *val;

	val = json_object_get(jwk, "kty");
	if (val == NULL || !json_is_string(val)) {
		fprintf(stderr, "JSON Object %d is not a valid JWK\n", count);
		return;
	}

	kty = json_string_value(val);

	if (!strcmp(kty, "EC")) {
		__process_ec_jwk(jwk);
	} else if (!strcmp(kty, "RSA")) {
		__process_rsa_jwk(jwk);
	} else if (!strcmp(kty, "OKP")) {
		__process_eddsa_jwk(jwk);
	} else {
		fprintf(stderr, "Unknown JWK key type %s\n", kty);
		return;
	}

	count++;
}

int main(int argc, char **argv)
{
	json_auto_t *jwk_set = NULL;
	json_t *jwk_array, *jwk;
	json_error_t error;
	char *file;
	size_t i;

	if (argc != 2) {
		fprintf(stderr, "Usage: %s <JWK(S) file>\n", argv[0]);
		exit(EXIT_FAILURE);
	}

	file = argv[1];

	mkdir("pems", 0755);

	fprintf(stderr, "Parsing %s\n", file);

	jwk_set = json_load_file(file, JSON_DECODE_ANY, &error);
	if (jwk_set == NULL) {
		fprintf(stderr, "ERROR: %s\n", error.text);
		exit(EXIT_FAILURE);
	}

	/* Check for "keys" as in a JWKS */
	jwk_array = json_object_get(jwk_set, "keys");
	if (jwk_array == NULL) {
		/* Assume a single JSON Object for one JWK */
		fprintf(stderr, "No keys found, processing as a single JWK\n");
		__process_one_jwk(jwk_set);
		exit(EXIT_SUCCESS);
	}

	fprintf(stderr, "Found %lu 'keys' to process\n", json_array_size(jwk_array));
	json_array_foreach(jwk_array, i, jwk) {
		__process_one_jwk(jwk);
	}

	fprintf(stderr, "Processing results:\n");
	if (ec_count)
		fprintf(stderr, "  EC     : %d\n", ec_count);
	if (rsa_count)
		fprintf(stderr, "  RSA    : %d\n", rsa_count);
	if (rsa_pss_count)
		fprintf(stderr, "  RSA-PSS: %d\n", rsa_pss_count);
	if (eddsa_count)
		fprintf(stderr, "  EdDSA  : %d\n", eddsa_count);
	fprintf(stderr, "\n");

	exit(EXIT_SUCCESS);
}
