/**********************************************************************************
 * This file is CONFIDENTIAL and any use by you is subject to the terms of the
 * agreement between you and Arm China or the terms of the agreement between you
 * and the party authorised by Arm China to disclose this file to you.
 * The confidential and proprietary information contained in this file
 * may only be used by a person authorised under and to the extent permitted
 * by a subsisting licensing agreement from Arm China.
 *
 *        (C) Copyright 2020 Arm Technology (China) Co. Ltd.
 *                    All rights reserved.
 *
 * This entire notice must be reproduced on all copies of this file and copies of
 * this file may only be made by a person if such person is permitted to do so
 * under the terms of a subsisting license agreement from Arm China.
 *
 *********************************************************************************/

/**
 * @file  aipu_arch.h
 * @brief AIPU User Mode Driver (UMD) AIPU arch module header
 */

#ifndef _AIPU_ARCH_H_
#define _AIPU_ARCH_H_

#include <stdint.h>

typedef struct aipu_architecture {
    char     sys_info[20];
    uint32_t vreg_num;
    uint32_t lsram_size_kb;
    uint32_t lsram_bank;
    uint32_t gsram_size_kb;
    uint32_t gsram_bank;
    uint32_t dma_chan;
    char     has_iram[10];
    char     has_dcache[10];
    uint32_t icache_size;
    uint32_t hwa_mac;
    uint32_t tec_num;
} aipu_arch_t;

/* architecture */
#define AIPU_HW_ARCH_ZHOUYI         0

/* version */
#define AIPU_HW_VERSION_ZHOUYI_V1   1
#define AIPU_HW_VERSION_ZHOUYI_V2   2

/* used only for z1 simulation */
#define AIPU_CONFIG_0904            904   /* Z1 SC000 */
#define AIPU_CONFIG_1002            1002  /* Z1 SC002 */
#define AIPU_CONFIG_0701            701   /* Z1 SC003 */
#define AIPU_ARCH_CONFIG_MAX        3

/**
 * @brief This function translates ISA version number read from AIPU register to AIPU HW version number
 *
 * @param isa_version ISA version register number
 *
 * @retval AIPU HW version number or 0 if failed
 */
inline uint32_t get_aipu_hw_version(uint32_t isa_version)
{
    if (isa_version == 0)
    {
        return AIPU_HW_VERSION_ZHOUYI_V1;
    }
    else if (isa_version == 1)
    {
        return AIPU_HW_VERSION_ZHOUYI_V2;
    }
    else
    {
        return 0;
    }
}
/**
 * @brief This function returns AIPU hardware configuration version number
 *
 * @param isa_version  ISA version register number
 * @param aiff_feature AIFF feature register number
 * @param tpc_feature  TPC feature register number
 *
 * @retval AIPU HW config number or 0 if failed
 */
inline uint32_t get_aipu_hw_config_version_number(uint32_t isa_version, uint32_t aiff_feature, uint32_t tpc_feature)
{
    uint32_t high = 0;
    uint32_t low = 0;

    if (isa_version == 0)
    {
        high = (aiff_feature & 0xF) + 6;
    }
    else if (isa_version == 1)
    {
        high = (aiff_feature & 0xF) + 8;
    }

    low = (tpc_feature) & 0x1F;

    return high * 100 + low;
}

#endif /* _AIPU_ARCH_H_ */