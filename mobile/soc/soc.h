/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 FreeBSD Mobile Project
 * All rights reserved.
 *
 * SoC abstraction framework for mobile platforms.
 */

#ifndef _MOBILE_SOC_H_
#define _MOBILE_SOC_H_

#include <sys/types.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Vendor identification */
#define SOC_VENDOR_MEDIATEK    0
#define SOC_VENDOR_QUALCOMM    1
#define SOC_VENDOR_SAMSUNG     2
#define SOC_VENDOR_GOOGLE      3
#define SOC_VENDOR_APPLE       4

typedef enum {
    SOC_VENDOR_UNKNOWN = -1,
    SOC_VENDOR_MEDIA_TEK = SOC_VENDOR_MEDIATEK,
    SOC_VENDOR_QUAL_COMM = SOC_VENDOR_QUALCOMM,
    SOC_VENDOR_SAMSUNG = SOC_VENDOR_SAMSUNG,
    SOC_VENDOR_GOOGLE = SOC_VENDOR_GOOGLE,
    SOC_VENDOR_APPLE = SOC_VENDOR_APPLE
} soc_vendor_t;

/* SoC model identification */
typedef enum {
    /* MediaTek Dimensity series */
    SOC_MODEL_MT6893 = 0,      /* Dimensity 1100 */
    SOC_MODEL_MT6891,          /* Dimensity 1200 */
    SOC_MODEL_MT6877,          /* Dimensity 920 */
    SOC_MODEL_MT6895,          /* Dimensity 9000 */
    SOC_MODEL_MT6983,          /* Dimensity 9100 */
    SOC_MODEL_MT6985,          /* Dimensity 9200 */
    SOC_MODEL_MT6989,          /* Dimensity 9300 */
    SOC_MODEL_MT6991,          /* Dimensity 9400 */
    SOC_MODEL_MT6833,          /* Dimensity 800U/720 */
    SOC_MODEL_MT6769,          /* Helio G90/G95 */
    SOC_MODEL_MT6768,          /* Helio G80 */
    SOC_MODEL_MT6765,          /* Helio G25 */
    SOC_MODEL_MT6739,          /* Helio entry-level */
    SOC_MODEL_MT6762,          /* Helio G35 */

    /* Qualcomm Snapdragon compute platforms */
    SOC_MODEL_SM8450,          /* Snapdragon 8cx Gen 3 */
    SOC_MODEL_SM8350,          /* Snapdragon 8cx Gen 2 */
    SOC_MODEL_SM8475,          /* Snapdragon 8+ Gen 1 */
    SOC_MODEL_SM8550,          /* Snapdragon 8 Gen 2 */
    SOC_MODEL_SM7450,          /* Snapdragon 7cx Gen 3 */
    SOC_MODEL_SM7350,          /* Snapdragon 7 Gen 1 */
    SOC_MODEL_SM6375,          /* Snapdragon 7s Gen 2 */
    SOC_MODEL_SM6450,          /* Snapdragon 6 Gen 1 */
    SOC_MODEL_SM6625,          /* Snapdragon 4 Gen 2 */
    SOC_MODEL_MSM8998,         /* Snapdragon 835 */
    SOC_MODEL_MSM8996,         /* Snapdragon 821/820 */

    /* Samsung Exynos series */
    SOC_MODEL_EXYNOS2200,      /* Exynos 2200 (S5E9925) */
    SOC_MODEL_EXYNOS2100,      /* Exynos 2100 (S5E9810) */
    SOC_MODEL_EXYNOS1380,      /* Exynos 1380 (S5E8845) */
    SOC_MODEL_EXYNOS1330,      /* Exynos 1330 (S5E8535) */
    SOC_MODEL_EXYNOS990,       /* Exynos 990 (S5E9830) */
    SOC_MODEL_EXYNOS9825,      /* Exynos 9825 (S5E9825) */
    SOC_MODEL_EXYNOS850,       /* Exynos 850 (S5E5510) */

    /* Google Tensor */
    SOC_MODEL_TENSOR_G3,       /* Tensor G3 (Zuma) */
    SOC_MODEL_TENSOR_G2,       /* Tensor G2 */
    SOC_MODEL_TENSOR_G1,       /* Tensor (Redfin) */

    /* Apple Silicon */
    SOC_MODEL_A17_PRO,         /* Apple A17 Pro */
    SOC_MODEL_A16,             /* Apple A16 */
    SOC_MODEL_A15,             /* Apple A15 (T8101) */
    SOC_MODEL_A14,             /* Apple A14 (T8100) */

    SOC_MODEL_MAX
} soc_model_t;

/* SoC descriptor structure */
typedef struct soc_desc {
    soc_vendor_t vendor;
    soc_model_t model;
    const char *name;
    int cpu_cores;
    uint32_t max_freq_mhz;
    const char *compatible;
} soc_desc_t;

/* Detection functions */
soc_vendor_t soc_detect(void);
soc_model_t soc_get_model(void);
const char *soc_get_name(void);
int soc_get_cpus(void);
uint32_t soc_get_max_freq(void);
const soc_desc_t *soc_get_desc(void);

/* Initialization hooks */
int soc_early_init(void);
int soc_init(void);
int soc_late_init(void);
int soc_boot_flow(void);

#ifdef __cplusplus
}
#endif

#endif /* _MOBILE_SOC_H_ */