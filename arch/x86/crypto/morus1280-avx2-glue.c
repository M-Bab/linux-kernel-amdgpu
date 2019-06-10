// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * The MORUS-1280 Authenticated-Encryption Algorithm
 *   Glue for AVX2 implementation
 *
 * Copyright (c) 2016-2018 Ondrej Mosnacek <omosnacek@gmail.com>
 * Copyright (C) 2017-2018 Red Hat, Inc. All rights reserved.
 */

#include <crypto/internal/aead.h>
#include <crypto/internal/simd.h>
#include <crypto/morus1280_glue.h>
#include <linux/module.h>
#include <asm/fpu/api.h>
#include <asm/cpu_device_id.h>

asmlinkage void crypto_morus1280_avx2_init(void *state, const void *key,
					   const void *iv);
asmlinkage void crypto_morus1280_avx2_ad(void *state, const void *data,
					 unsigned int length);

asmlinkage void crypto_morus1280_avx2_enc(void *state, const void *src,
					  void *dst, unsigned int length);
asmlinkage void crypto_morus1280_avx2_dec(void *state, const void *src,
					  void *dst, unsigned int length);

asmlinkage void crypto_morus1280_avx2_enc_tail(void *state, const void *src,
					       void *dst, unsigned int length);
asmlinkage void crypto_morus1280_avx2_dec_tail(void *state, const void *src,
					       void *dst, unsigned int length);

asmlinkage void crypto_morus1280_avx2_final(void *state, void *tag_xor,
					    u64 assoclen, u64 cryptlen);

MORUS1280_DECLARE_ALG(avx2, "morus1280-avx2", 400);

static struct simd_aead_alg *simd_alg;

static int __init crypto_morus1280_avx2_module_init(void)
{
	if (!boot_cpu_has(X86_FEATURE_AVX2) ||
	    !boot_cpu_has(X86_FEATURE_OSXSAVE) ||
	    !cpu_has_xfeatures(XFEATURE_MASK_SSE | XFEATURE_MASK_YMM, NULL))
		return -ENODEV;

	return simd_register_aeads_compat(&crypto_morus1280_avx2_alg, 1,
					  &simd_alg);
}

static void __exit crypto_morus1280_avx2_module_exit(void)
{
	simd_unregister_aeads(&crypto_morus1280_avx2_alg, 1, &simd_alg);
}

module_init(crypto_morus1280_avx2_module_init);
module_exit(crypto_morus1280_avx2_module_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Ondrej Mosnacek <omosnacek@gmail.com>");
MODULE_DESCRIPTION("MORUS-1280 AEAD algorithm -- AVX2 implementation");
MODULE_ALIAS_CRYPTO("morus1280");
MODULE_ALIAS_CRYPTO("morus1280-avx2");
