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
 * @brief AIPU UMD test implementation file: pipeline test
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
#include <pthread.h>
#include <atomic>
#include "standard_api.h"
#include "common/common.h"
#include "common/multithread/scheduling_thread.h"
#include "common/multithread/job_status_poll.h"

 /**
  * In this test, multithreads share one common context and common graph descriptor(s).
  */

using namespace std;
const char* test_case = "multithread";

#define SCHED_THREAD_CNT 3
#define PIPELINE_CNT     3
#define LOOP_CNT         20

aipu_ctx_handle_t* ctx;
std::atomic<uint32_t> stop(0);
graph_test_info_t* test_info = nullptr;
pipe_job_scheduling_data_t sched_data[SCHED_THREAD_CNT];
job_status_poll_data_t poll_data;

int main(int argc, char* argv[])
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    int pass = 0;
    uint32_t graph_cnt = 1;
    uint32_t tot_job_cnt = PIPELINE_CNT * SCHED_THREAD_CNT;
    uint32_t loop_cnt = LOOP_CNT;
    const char* status_msg = nullptr;
    pthread_t sched_tid[SCHED_THREAD_CNT];
    pthread_t poll_tid;
    aipu_runtime_config_t rt_config;

    if (argc < 3)
    {
        fprintf(stderr, "[TEST ERROR] need more options (use -h to find available options)!\n");
        goto finish;
    }

    test_info = create_gtest_info(argc, argv, test_case, graph_cnt, tot_job_cnt);
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

    rt_config.poll_opt = true;
    ret = AIPU_set_runtime_config(ctx, &rt_config);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_set_runtime_config: %s\n", status_msg);
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

    for (uint32_t i = 0; i < SCHED_THREAD_CNT; i++)
    {
        /* create SCHED_THREAD_CNT scheduling threads */
        /* every thread use pipeline of count PIPELINE_CNT */
        /* every pipeline executes loop_cnt times */
        sched_data[i].ctx = ctx;
        sched_data[i].test_info = test_info;
        sched_data[i].test_info_iter = 0;
        sched_data[i].use_pipeline = 1;
        sched_data[i].pipe_cnt = PIPELINE_CNT;
        sched_data[i].job_start_iter = i * PIPELINE_CNT;
        sched_data[i].loop_cnt = loop_cnt;
        sched_data[i].stop = &stop;
        if ((pthread_create(&sched_tid[i], NULL, pipeline_job_scheduling_thread, &sched_data[i])) == -1)
        {
            fprintf(stderr, "[TEST ERROR] create sched thread failed!\n");
            goto clean_graph;
        }
    }

    poll_data.ctx = ctx;
    poll_data.stop = &stop;
    poll_data.stop_cnt = SCHED_THREAD_CNT;
    if ((pthread_create(&poll_tid, NULL, job_status_poll, &poll_data)) == -1)
    {
        fprintf(stderr, "[TEST ERROR] create poll thread failed!\n");
        goto clean_graph;
    }

    for (uint32_t i = 0; i < SCHED_THREAD_CNT; i++)
    {
        if (pthread_join(sched_tid[i], NULL) != 0)
        {
            fprintf(stderr, "[TEST ERROR] join sched thread #%u failed!\n", i);
        }
        else
        {
            fprintf(stdout, "[TEST INFO] join sched thread #%u successfully.\n", i);
        }
    }

    if (pthread_join(poll_tid, NULL) != 0)
    {
        fprintf(stderr, "[TEST ERROR] join poll thread failed!\n");
    }
    else
    {
        fprintf(stdout, "[TEST INFO] join poll thread successfully.\n");
    }

clean_graph:
    ret = AIPU_unload_graph(ctx, &test_info[0].gdesc);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_unload_graph: %s\n", status_msg);
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