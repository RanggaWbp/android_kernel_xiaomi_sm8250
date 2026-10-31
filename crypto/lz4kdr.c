/*
 * Cryptographic API.
 *
 * LZ4KDR compression algorithm (speed-tuned LZ4KD derivative).
 * See include/linux/lz4kdr.h for attribution.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/crypto.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/lz4kdr.h>
#include <crypto/internal/scompress.h>

#define LZ4KDR_MEM_COMPRESS	lz4kdr_encode_state_bytes_min()

struct lz4kdr_ctx {
	void *lz4kdr_comp_mem;
};

static void *lz4kdr_alloc_ctx(struct crypto_scomp *tfm)
{
	void *ctx;

	ctx = kzalloc(LZ4KDR_MEM_COMPRESS, GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	return ctx;
}

static int lz4kdr_init(struct crypto_tfm *tfm)
{
	struct lz4kdr_ctx *ctx = crypto_tfm_ctx(tfm);

	ctx->lz4kdr_comp_mem = lz4kdr_alloc_ctx(NULL);
	if (IS_ERR(ctx->lz4kdr_comp_mem))
		return -ENOMEM;

	return 0;
}

static void lz4kdr_free_ctx(struct crypto_scomp *tfm, void *ctx)
{
	kfree(ctx);
}

static void lz4kdr_exit(struct crypto_tfm *tfm)
{
	struct lz4kdr_ctx *ctx = crypto_tfm_ctx(tfm);

	lz4kdr_free_ctx(NULL, ctx->lz4kdr_comp_mem);
}

static int __lz4kdr_compress_crypto(const u8 *src, unsigned int slen,
				    u8 *dst, unsigned int *dlen, void *ctx)
{
	int out_len;

	out_len = lz4kdr_encode(ctx, src, dst, slen, *dlen, 0);
	if (out_len < 0)
		return -EINVAL;
	/* 0 = incompressible -> report as error so zram keeps original */
	if (!out_len)
		return -EINVAL;

	*dlen = out_len;
	return 0;
}

static int lz4kdr_scompress(struct crypto_scomp *tfm, const u8 *src,
			    unsigned int slen, u8 *dst, unsigned int *dlen,
			    void *ctx)
{
	return __lz4kdr_compress_crypto(src, slen, dst, dlen, ctx);
}

static int lz4kdr_compress_crypto(struct crypto_tfm *tfm, const u8 *src,
				  unsigned int slen, u8 *dst,
				  unsigned int *dlen)
{
	struct lz4kdr_ctx *ctx = crypto_tfm_ctx(tfm);

	return __lz4kdr_compress_crypto(src, slen, dst, dlen,
					ctx->lz4kdr_comp_mem);
}

static int __lz4kdr_decompress_crypto(const u8 *src, unsigned int slen,
				      u8 *dst, unsigned int *dlen, void *ctx)
{
	int out_len;

	out_len = lz4kdr_decode(src, dst, slen, *dlen);
	if (out_len < 0)
		return -EINVAL;

	*dlen = out_len;
	return 0;
}

static int lz4kdr_sdecompress(struct crypto_scomp *tfm, const u8 *src,
			      unsigned int slen, u8 *dst, unsigned int *dlen,
			      void *ctx)
{
	return __lz4kdr_decompress_crypto(src, slen, dst, dlen, NULL);
}

static int lz4kdr_decompress_crypto(struct crypto_tfm *tfm, const u8 *src,
				    unsigned int slen, u8 *dst,
				    unsigned int *dlen)
{
	return __lz4kdr_decompress_crypto(src, slen, dst, dlen, NULL);
}

static struct crypto_alg alg_lz4kdr = {
	.cra_name		= "lz4kdr",
	.cra_flags		= CRYPTO_ALG_TYPE_COMPRESS,
	.cra_ctxsize		= sizeof(struct lz4kdr_ctx),
	.cra_module		= THIS_MODULE,
	.cra_init		= lz4kdr_init,
	.cra_exit		= lz4kdr_exit,
	.cra_u			= { .compress = {
	.coa_compress		= lz4kdr_compress_crypto,
	.coa_decompress		= lz4kdr_decompress_crypto } }
};

static struct scomp_alg scomp = {
	.alloc_ctx		= lz4kdr_alloc_ctx,
	.free_ctx		= lz4kdr_free_ctx,
	.compress		= lz4kdr_scompress,
	.decompress		= lz4kdr_sdecompress,
	.base			= {
		.cra_name	= "lz4kdr",
		.cra_driver_name = "lz4kdr-scomp",
		.cra_module	 = THIS_MODULE,
	}
};

static int __init lz4kdr_mod_init(void)
{
	int ret;

	ret = crypto_register_alg(&alg_lz4kdr);
	if (ret)
		return ret;

	ret = crypto_register_scomp(&scomp);
	if (ret) {
		crypto_unregister_alg(&alg_lz4kdr);
		return ret;
	}

	return ret;
}

static void __exit lz4kdr_mod_fini(void)
{
	crypto_unregister_alg(&alg_lz4kdr);
	crypto_unregister_scomp(&scomp);
}

module_init(lz4kdr_mod_init);
module_exit(lz4kdr_mod_fini);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("LZ4KDR Compression Algorithm");
MODULE_ALIAS_CRYPTO("lz4kdr");
