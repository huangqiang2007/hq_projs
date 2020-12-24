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
 * @file  main.cpp
 * @brief AIPU UMD test implementation file: benchmark test
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <iostream>
#include <string.h>
#include <errno.h>
#include <vector>
#include "low_level_api.h"

using namespace std;
const char* test_case = "low_level_api";

#define AIPU_EXT_REG_OFFSET_STATUS_REG 0x4
#define AIPU_EXT_REG_OFFSET_DATA0_REG  0x14

int main(int argc, char* argv[])
{
    aipu_ll_status_t ret = AIPU_LL_STATUS_SUCCESS;
    AIPU_HANDLE handle;
    uint32_t isa_version = 0;
    uint32_t reg_val = 0;
    aipu_buffer_t buf;
    int pass = 0;

    /* open test */
    ret = AIPU_LL_open(&handle);
    if (AIPU_LL_STATUS_SUCCESS != ret)
    {
        fprintf(stderr, "[UMD ERROR] open dev failed: 0x%x.\n", ret);
        pass = -1;
        goto finish;
    }
    fprintf(stdout, "[UMD TEST] open dev successfully.\n");

    /* get ISA version test */
    ret = AIPU_LL_get_hw_version(handle, &isa_version);
    if (AIPU_LL_STATUS_SUCCESS != ret)
    {
        fprintf(stderr, "[UMD ERROR] get dev version failed: 0x%x.\n", ret);
        pass = -1;
        goto finish;
    }
    fprintf(stdout, "[UMD TEST] AIPU ISA version: 0x%x.\n", isa_version);

    /* status register read test */
    ret = AIPU_LL_read_reg32(handle, AIPU_EXT_REG_OFFSET_STATUS_REG, &reg_val);
    if (AIPU_LL_STATUS_SUCCESS != ret)
    {
        fprintf(stderr, "[UMD ERROR] read status reg failed: 0x%x.\n", ret);
        pass = -1;
        goto finish;
    }
    fprintf(stdout, "[UMD TEST] AIPU status register (0x4): 0x%x.\n", reg_val);

    /* register write test */
    /* read initial value first */
    ret = AIPU_LL_read_reg32(handle, AIPU_EXT_REG_OFFSET_DATA0_REG, &reg_val);
    if (AIPU_LL_STATUS_SUCCESS != ret)
    {
        fprintf(stderr, "[UMD ERROR] read data0 reg failed: 0x%x.\n", ret);
        pass = -1;
        goto finish;
    }
    fprintf(stdout, "[UMD TEST] read data0 register (0x4) init val: 0x%x.\n", reg_val);
    /* write */
    ret = AIPU_LL_write_reg32(handle, AIPU_EXT_REG_OFFSET_DATA0_REG, 0xAABBCDEF);
    if (AIPU_LL_STATUS_SUCCESS != ret)
    {
        fprintf(stderr, "[UMD ERROR] write data0 reg failed: 0x%x.\n", ret);
        pass = -1;
        goto finish;
    }
    fprintf(stdout, "[UMD TEST] write register (0x4) with: 0x%x.\n", 0xAABBCDEF);
    /* read back */
    ret = AIPU_LL_read_reg32(handle, AIPU_EXT_REG_OFFSET_DATA0_REG, &reg_val);
    if (AIPU_LL_STATUS_SUCCESS != ret)
    {
        fprintf(stderr, "[UMD ERROR] read data0 reg failed: 0x%x.\n", ret);
        pass = -1;
        goto finish;
    }
    fprintf(stdout, "[UMD TEST] read back data0 register (0x4): 0x%x.\n", reg_val);

    /* mem allocation test */
    ret = AIPU_LL_malloc(handle, 1024, 4, &buf);
    if (AIPU_LL_STATUS_SUCCESS != ret)
    {
        fprintf(stderr, "[UMD ERROR] memory allocation failed: 0x%x.\n", ret);
        pass = -1;
        goto finish;
    }
    fprintf(stdout, "[UMD TEST] memory allocation successfully: base pa: 0x%lx, size 0x%lx.\n",
        buf.pa, buf.size);

    /* initial value read */
    fprintf(stdout, "[UMD TEST] initial value at pa: 0x%lx is 0x%x.\n",
        buf.pa, *(uint32_t*)buf.va);
    /* write */
    *(uint32_t*)buf.va = 0x12345678;
    fprintf(stdout, "[UMD TEST] write 0x%x at pa: 0x%lx.\n",
        0x12345678, buf.pa);
    /* read back */
    fprintf(stdout, "[UMD TEST] read back value at pa: 0x%lx is 0x%x.\n",
        buf.pa, *(uint32_t*)buf.va);

    ret = AIPU_LL_free(handle, &buf);
    if (AIPU_LL_STATUS_SUCCESS != ret)
    {
        fprintf(stderr, "[UMD ERROR] memory free failed: 0x%x.\n", ret);
        pass = -1;
        goto finish;
    }
    fprintf(stdout, "[UMD TEST] memory free successfully: base pa: 0x%lx, size 0x%lx.\n",
        buf.pa, buf.size);

    ret = AIPU_LL_close(handle);
    if (AIPU_LL_STATUS_SUCCESS != ret)
    {
        fprintf(stderr, "[UMD ERROR] close dev failed: 0x%x.\n", ret);
        pass = -1;
        goto finish;
    }
    fprintf(stdout, "[UMD TEST] close dev successfully.\n");

finish:
    return pass;
}