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
 * @brief AIPU UMD test implementation file: benchmark simulation test
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <iostream>
#include <string>
#include <cstring>
#include <errno.h>
#include <vector>
#include "standard_api.h"
#include "common/common.h"

using namespace std;
const char* test_case = "simulation";

int main(int argc, char* argv[])
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    int pass = 0;
    uint32_t graph_cnt = 1;
    uint32_t pipe_cnt = 1;
    int32_t time_out = 100000;
    graph_test_info_t* test_info = nullptr;

    aipu_ctx_handle_t* ctx;
    const char* status_msg = nullptr;
    aipu_simulation_config_t config;

    /* more options can be added, such as
     * std::string sim_opt("--mem_hist=sim_mem_hist.txt --enable-trace");
     */
    std::string sim_opt("--mem_hist=sim_mem_hist.txt");

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
        fprintf(stderr, "[TEST ERROR] AIPU_init_ctx: %s\n", status_msg);
        goto finish;
    }

    config.simulator = test_info[0].opt.simulator;
    config.cfg_file_dir = test_info[0].opt.cfg_file_dir;
    config.output_dir = test_info[0].opt.dump_dir;
    config.simulator_opt = sim_opt.c_str(); /* should be set to be NULL if unused */
    ret = AIPU_config_simulation(ctx, &config);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_config_simulation: %s\n", status_msg);
        goto finish;
    }

    ret = AIPU_load_graph_helper(ctx, test_info[0].bench.graph_fname.c_str(), &test_info[0].gdesc);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_load_graph_helper: %s\n", status_msg);
        goto finish;
    }
    fprintf(stdout, "[TEST INFO] AIPU load graph successfully.\n");

    ret = AIPU_alloc_tensor_buffers(ctx, &test_info[0].gdesc, &test_info[0].jobs[0].buffer);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_alloc_tensor_buffers: %s\n", status_msg);
        goto clean_graph;
    }

    load_inputs(test_info[0], 0);
    ret = AIPU_create_job(ctx, &test_info[0].gdesc, test_info[0].jobs[0].buffer.handle,
        &test_info[0].jobs[0].id);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_create_job: %s\n", status_msg);
        goto clean_buffer;
    }

    ret = AIPU_finish_job(ctx, test_info[0].jobs[0].id, time_out);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_finish_job: %s\n", status_msg);
        pass = -1;
        goto clean_job;
    }

    pass = check_result_pass(test_info[0], test_info[0].jobs[0].id);

clean_job:
    ret = AIPU_clean_job(ctx, test_info[0].jobs[0].id);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_clean_job: %s\n", status_msg);
        goto finish;
    }

clean_buffer:
    ret = AIPU_free_tensor_buffers(ctx, test_info[0].jobs[0].buffer.handle);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_free_tensor_buffers: %s\n", status_msg);
        goto finish;
    }

clean_graph:
    ret = AIPU_unload_graph(ctx, &test_info[0].gdesc);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_unload_graph; %s\n", status_msg);
        goto finish;
    }

    ret = AIPU_deinit_ctx(ctx);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_deinit_ctx: %s\n", status_msg);
        goto finish;
    }

finish:
    destroy_gtest_info(test_info, graph_cnt);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        pass = -1;
    }
    return pass;
}