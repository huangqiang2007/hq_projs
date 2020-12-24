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
#include "standard_api.h"
#include "common/common.h"

using namespace std;
const char* test_case = "debugger";

int main(int argc, char* argv[])
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    int pass = 0;
    uint32_t graph_cnt = 1;
    uint32_t pipe_cnt = 1;
    graph_test_info_t* test_info = nullptr;

    aipu_ctx_handle_t* ctx;
    const char* status_msg = nullptr;
    uint32_t aipu_status = 0;
    aipu_debug_info_t dbg_info;

    aipu_runtime_config_t rt_config;

    if (argc < 3)
    {
        fprintf(stderr, "[TEST ERROR] need more options (use -h to find available options)!\n");
        goto finish;
    }

    test_info = create_gtest_info(argc, argv, test_case, graph_cnt, pipe_cnt);
    if (nullptr == test_info)
    {
        fprintf(stderr, "[TEST ERROR] create test info failed!\n");
        goto finish;
    }

    ret = AIPU_init_ctx(&ctx);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] %s\n", status_msg);
        goto finish;
    }

    /**
     * set [bypass_version_check == 1] if debugger would like to bypass version checking and run
     * unmatched aipu.bin binary on target AIPU hardware
     */
    rt_config.poll_opt = 0;
    rt_config.bypass_version_check = 0;
    ret = AIPU_set_runtime_config(ctx, &rt_config);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_set_runtime_config: %s\n", status_msg);
        goto finish;
    }

    /* This API can be called immediately after init successfully returns */
    ret = AIPU_get_dev_status(ctx, &aipu_status);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] %s\n", status_msg);
        goto finish;
    }
    fprintf(stdout, "[TEST INFO] aipu hardware status: 0x%x (after init)\n", aipu_status);

    ret = AIPU_load_graph_helper(ctx, test_info[0].bench.graph_fname.c_str(), &test_info[0].gdesc);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] %s\n", status_msg);
        goto finish;
    }
    fprintf(stdout, "[TEST INFO] AIPU load graph successfully.\n");

    ret = AIPU_alloc_tensor_buffers(ctx, &test_info[0].gdesc, &test_info[0].jobs[0].buffer);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] %s\n", status_msg);
        goto clean_graph;
    }

    load_inputs(test_info[0], 0);
    ret = AIPU_create_job(ctx, &test_info[0].gdesc, test_info[0].jobs[0].buffer.handle,
        &test_info[0].jobs[0].id);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] %s\n", status_msg);
        goto clean_buffer;
    }

    /* This API should be called after AIPU_create_job and before AIPU_finish_job/AIPU_flush_job */
    ret = AIPU_get_debug_info(ctx, test_info[0].jobs[0].id, &dbg_info);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] %s\n", status_msg);
        goto finish;
    }
    fprintf(stdout, "[TEST INFO] aipu job debugging info:\n");
    fprintf(stdout, "[TEST INFO] -job ID: 0x%x\n", test_info[0].jobs[0].id);
    fprintf(stdout, "[TEST INFO] -instruction base: 0x%lx\n", dbg_info.instruction_base_pa);
    fprintf(stdout, "[TEST INFO] -start PC: 0x%lx\n", dbg_info.start_pc_pa);
    fprintf(stdout, "[TEST INFO] -IPC: 0x%lx\n", dbg_info.interrupt_pc_pa);

    ret = AIPU_finish_job(ctx, test_info[0].jobs[0].id, -1);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] %s\n", status_msg);
        pass = -1;
        goto clean_job;
    }

    ret = AIPU_get_dev_status(ctx, &aipu_status);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] %s\n", status_msg);
        goto finish;
    }
    fprintf(stdout, "[TEST INFO] aipu hardware status: 0x%x (after finish job)\n", aipu_status);

    pass = check_result_pass(test_info[0], test_info[0].jobs[0].id);

clean_job:
    ret = AIPU_clean_job(ctx, test_info[0].jobs[0].id);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] %s\n", status_msg);
    }
    fprintf(stdout, "[TEST INFO] job cleaned.\n");

clean_buffer:
    ret = AIPU_free_tensor_buffers(ctx, test_info[0].jobs[0].buffer.handle);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] %s\n", status_msg);
    }
    fprintf(stdout, "[TEST INFO] buffer freed.\n");

clean_graph:
    ret = AIPU_unload_graph(ctx, &test_info[0].gdesc);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] %s\n", status_msg);
    }
    fprintf(stdout, "[TEST INFO] graph unloaded.\n");

    AIPU_deinit_ctx(ctx);
    fprintf(stdout, "[TEST INFO] context deinit.\n");

finish:
    destroy_gtest_info(test_info, graph_cnt);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        pass = -1;
    }
    return pass;
}