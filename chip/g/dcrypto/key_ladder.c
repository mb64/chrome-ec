/* Copyright 2017 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#include "dcrypto.h"
#include "internal.h"
#include "endian.h"
#include "registers.h"

static void ladder_init(void)
{
	/* Do not reset keyladder engine here, as before.
	 *
	 * Should not be needed and if it is, it is indicative
	 * of sync error between this and sha engine usage.
	 * Reset will make this flow work, but will have broken
	 * the other pending sha flow.
	 * Hence leave as is and observe the error.
	 */
}

static int ladder_step(uint32_t cert, const uint32_t input[8])
{
	GREG32(KEYMGR, SHA_ITOP) = 0;  /* clear status */

	GREG32(KEYMGR, SHA_USE_CERT_INDEX) =
		(cert << GC_KEYMGR_SHA_USE_CERT_INDEX_LSB) |
		GC_KEYMGR_SHA_USE_CERT_ENABLE_MASK;

	GREG32(KEYMGR, SHA_CFG_EN) =
		GC_KEYMGR_SHA_CFG_EN_INT_EN_DONE_MASK;
	GREG32(KEYMGR, SHA_TRIG) =
		GC_KEYMGR_SHA_TRIG_TRIG_GO_MASK;

	if (input) {
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[0];
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[1];
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[2];
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[3];
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[4];
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[5];
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[6];
		GREG32(KEYMGR, SHA_INPUT_FIFO) = input[7];

		GREG32(KEYMGR, SHA_TRIG) = GC_KEYMGR_SHA_TRIG_TRIG_STOP_MASK;
	}

	while (!GREG32(KEYMGR, SHA_ITOP))
		;

	GREG32(KEYMGR, SHA_ITOP) = 0;  /* clear status */

	return !!GREG32(KEYMGR, HKEY_ERR_FLAGS);
}

static int compute_certs(const uint32_t *certs, size_t num_certs)
{
	int i;

	for (i = 0; i < num_certs; i++) {
		if (ladder_step(certs[i], NULL))
			return 0;
	}

	return 1;
}

#define KEYMGR_CERT_0 0
#define KEYMGR_CERT_3 3
#define KEYMGR_CERT_4 4
#define KEYMGR_CERT_5 5
#define KEYMGR_CERT_7 7
#define KEYMGR_CERT_15 15
#define KEYMGR_CERT_20 20
#define KEYMGR_CERT_25 25
#define KEYMGR_CERT_26 26
#define KEYMGR_CERT_34 34
#define KEYMGR_CERT_35 35

static const uint32_t FRK2_CERTS_PREFIX[] = {
	KEYMGR_CERT_0,
	KEYMGR_CERT_3,
	KEYMGR_CERT_4,
	KEYMGR_CERT_5,
	KEYMGR_CERT_7,
	KEYMGR_CERT_15,
	KEYMGR_CERT_20,
};

static const uint32_t FRK2_CERTS_POSTFIX[] = {
	KEYMGR_CERT_26,
};

#define MAX_MAJOR_FW_VERSION 254

int DCRYPTO_ladder_compute_frk2(size_t fw_version, uint8_t *frk2)
{
	int result = 0;

	if (fw_version > MAX_MAJOR_FW_VERSION)
		return 0;

	if (!dcrypto_grab_sha_hw())
		return 0;

	do {
		int i;

		ladder_init();

		if (!compute_certs(FRK2_CERTS_PREFIX,
					ARRAY_SIZE(FRK2_CERTS_PREFIX)))
			break;

		for (i = 0; i < MAX_MAJOR_FW_VERSION - fw_version; i++) {
			if (ladder_step(KEYMGR_CERT_25, NULL))
				break;
		}

		if (!compute_certs(FRK2_CERTS_POSTFIX,
					ARRAY_SIZE(FRK2_CERTS_POSTFIX)))
			break;

		memcpy(frk2, (void *) GREG32_ADDR(KEYMGR, HKEY_FRR0),
			AES256_BLOCK_CIPHER_KEY_SIZE);

		result = 1;
	} while (0);

	dcrypto_release_sha_hw();
	return result;
}

/* ISR salt (SHA256("ISR_SALT")) to use for USR generation. */
static const uint32_t ISR_SALT[8] = {
	0x6ba1b495, 0x4b7ca214, 0xfe07e922, 0x09735185,
	0xfcca43ca, 0xc6d4dfd9, 0x5fc2fcca, 0xaa45400b
};

/* Map of populated USR registers. */
static int usr_ready[8]  = {};

int dcrypto_ladder_compute_usr(enum dcrypto_appid id,
			       const uint32_t usr_salt[8])
{
	int result = 0;

	/* Check for USR readiness. */
	if (usr_ready[id])
		return 1;

	if (!dcrypto_grab_sha_hw())
		return 0;

	do {
		int i;

		/* The previous check performed without lock acquisition. */
		if (usr_ready[id]) {
			result = 1;
			break;
		}

		ladder_init();

		if (!compute_certs(FRK2_CERTS_PREFIX,
					ARRAY_SIZE(FRK2_CERTS_PREFIX)))
			break;

		/* USR generation requires running the key-ladder till
		 * the end (version 0), plus one additional iteration.
		 */
		for (i = 0; i < MAX_MAJOR_FW_VERSION - 0 + 1; i++) {
			if (ladder_step(KEYMGR_CERT_25, NULL))
				break;
		}
		if (i != MAX_MAJOR_FW_VERSION - 0 + 1)
			break;

		if (ladder_step(KEYMGR_CERT_34, ISR_SALT))
			break;

		/* Output goes to USR[appid] (the multiply by 2 is an
		 * artifact of slot addressing).
		 */
		GWRITE_FIELD(KEYMGR, SHA_CERT_OVERRIDE, DIGEST_PTR, 2 * id);
		if (ladder_step(KEYMGR_CERT_35, usr_salt))
			break;

		/* Check for key-ladder errors. */
		if (GREG32(KEYMGR, HKEY_ERR_FLAGS))
			break;

		/* Key deposited in USR[id], and ready to use. */
		usr_ready[id] = 1;

		result = 1;
	} while (0);

	dcrypto_release_sha_hw();
	return result;
}
