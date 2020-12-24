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
 * @file  scheduling_thread.cpp
 * @brief AIPU UMD test implementation file: job scheduling thread
 */

#include <stdio.h>
#include "scheduling_thread.h"

void* non_pipeline_job_scheduling_thread(void* data)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    pipe_job_scheduling_data_t* pdata = (pipe_job_scheduling_data_t*)data;
    const char* status_msg = nullptr;
    int pass = 0;
    uint32_t t_iter = pdata->test_info_iter;
    int32_t time_out = 500;

    if ((nullptr == pdata) || (nullptr == pdata->test_info))
    {
        goto finish;
    }

    ret = AIPU_alloc_tensor_buffers(pdata->ctx, &pdata->test_info[t_iter].gdesc,
        &pdata->test_info[t_iter].jobs[0].buffer);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_alloc_tensor_buffers: %s\n", status_msg);
        goto finish;
    }

    for (uint32_t loop = 0; loop < pdata->loop_cnt; loop++)
    {
        fprintf(stdout, "[TEST INFO] Loop #%u start...\n", loop);
        load_inputs(pdata->test_info[t_iter], 0);
        ret = AIPU_create_job(pdata->ctx, &pdata->test_info[t_iter].gdesc,
            pdata->test_info[t_iter].jobs[0].buffer.handle,
            &pdata->test_info[t_iter].jobs[0].id);
        if (ret != AIPU_STATUS_SUCCESS)
        {
            AIPU_get_status_msg(ret, &status_msg);
            fprintf(stderr, "[TEST ERROR] AIPU_create_job: %s\n", status_msg);
            goto clean_buffer;
        }

        ret = AIPU_finish_job(pdata->ctx, pdata->test_info[t_iter].jobs[0].id, time_out);
        if (ret != AIPU_STATUS_SUCCESS)
        {
            AIPU_get_status_msg(ret, &status_msg);
            fprintf(stderr, "[TEST ERROR] AIPU_flush_job: %s\n", status_msg);
            pass = -1;
            goto clean_buffer;
        }
        fprintf(stdout, "[TEST INFO] job #%u finished.\n", pdata->test_info[t_iter].jobs[0].id);

        if (check_result_pass(pdata->test_info[t_iter], pdata->test_info[t_iter].jobs[0].id) != 0)
        {
            pass = -1;
        }
        ret = AIPU_clean_job(pdata->ctx, pdata->test_info[t_iter].jobs[0].id);
        if (ret != AIPU_STATUS_SUCCESS)
        {
            AIPU_get_status_msg(ret, &status_msg);
            fprintf(stderr, "[TEST ERROR] AIPU_clean_job: %s\n", status_msg);
        }
        fprintf(stdout, "[TEST INFO] job #%u cleaned.\n", pdata->test_info[t_iter].jobs[0].id);
    }

    if (pass != 0)
    {
        fprintf(stderr, "[TEST ERROR] multithread non-pipeline test failed!\n");
    }
    else
    {
        fprintf(stderr, "[TEST INFO] multithread non-pipeline test pass.\n");
    }

clean_buffer:
    ret = AIPU_free_tensor_buffers(pdata->ctx, pdata->test_info[t_iter].jobs[0].buffer.handle);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] %s\n", status_msg);
    }
    fprintf(stdout, "[TEST INFO] buffer freed.\n");

finish:
    return data;
}

void* pipeline_job_scheduling_thread(void* data)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    pipe_job_scheduling_data_t* pdata = (pipe_job_scheduling_data_t*)data;
    const char* status_msg = nullptr;
    aipu_job_status_t job_status;
    int pass = 0;
    uint32_t t_iter = pdata->test_info_iter;

    if ((nullptr == pdata) || (nullptr == pdata->test_info))
    {
        goto finish;
    }

    for (uint32_t i = pdata->job_start_iter; i < (pdata->job_start_iter + pdata->pipe_cnt); i++)
    {
        ret = AIPU_alloc_tensor_buffers(pdata->ctx, &pdata->test_info[t_iter].gdesc,
            &pdata->test_info[t_iter].jobs[i].buffer);
        if (ret != AIPU_STATUS_SUCCESS)
        {
            AIPU_get_status_msg(ret, &status_msg);
            fprintf(stderr, "[TEST ERROR] AIPU_alloc_tensor_buffers: %s\n", status_msg);
            goto finish;
        }
        fprintf(stdout, "[TEST INFO] Alloc tensor buffer for job 0x%x, handle 0x%x.\n",
            i, pdata->test_info[t_iter].jobs[i].buffer.handle);
    }

    for (uint32_t loop = 0; loop < pdata->loop_cnt; loop++)
    {
        fprintf(stdout, "[TEST INFO] Loop #%u start...\n", loop);
        for (uint32_t i = pdata->job_start_iter; i < (pdata->job_start_iter + pdata->pipe_cnt); i++)
        {
            load_inputs(pdata->test_info[t_iter], i);
            ret = AIPU_create_job(pdata->ctx, &pdata->test_info[t_iter].gdesc,
                pdata->test_info[t_iter].jobs[i].buffer.handle,
                &pdata->test_info[t_iter].jobs[i].id);
            if (ret != AIPU_STATUS_SUCCESS)
            {
                AIPU_get_status_msg(ret, &status_msg);
                fprintf(stderr, "[TEST ERROR] AIPU_create_job: %s\n", status_msg);
                goto clean_buffer;
            }

            ret = AIPU_flush_job(pdata->ctx, pdata->test_info[t_iter].jobs[i].id);
            if (ret != AIPU_STATUS_SUCCESS)
            {
                AIPU_get_status_msg(ret, &status_msg);
                fprintf(stderr, "[TEST ERROR] AIPU_flush_job: %s\n", status_msg);
                pass = -1;
                goto clean_buffer;
            }
            fprintf(stdout, "[TEST INFO] job #%u scheduled.\n", pdata->test_info[t_iter].jobs[i].id);
        }

        for (uint32_t i = pdata->job_start_iter; i < (pdata->job_start_iter + pdata->pipe_cnt); i++)
        {
            ret = AIPU_get_job_status(pdata->ctx, pdata->test_info[t_iter].jobs[i].id, -1, &job_status);
            if ((ret != AIPU_STATUS_SUCCESS) || (job_status != AIPU_JOB_STATUS_DONE))
            {
                AIPU_get_status_msg(ret, &status_msg);
                fprintf(stderr, "[TEST ERROR] AIPU_get_job_status: %s\n", status_msg);
                pass = -1;
            }
            if (check_result_pass(pdata->test_info[t_iter], pdata->test_info[t_iter].jobs[i].id) != 0)
            {
                pass = -1;
            }
            ret = AIPU_clean_job(pdata->ctx, pdata->test_info[t_iter].jobs[i].id);
            if (ret != AIPU_STATUS_SUCCESS)
            {
                AIPU_get_status_msg(ret, &status_msg);
                fprintf(stderr, "[TEST ERROR] AIPU_clean_job: %s\n", status_msg);
            }
            fprintf(stdout, "[TEST INFO] job #%u cleaned.\n", pdata->test_info[t_iter].jobs[i].id);
        }
     }

    if (pass != 0)
    {
        fprintf(stderr, "[TEST ERROR] pipeline test failed!\n");
    }
    else
    {
        fprintf(stderr, "[TEST INFO] pipeline test pass.\n");
    }

    /* execution end and return */
    (*pdata->stop)++;

clean_buffer:
    for (uint32_t i = pdata->job_start_iter; i < (pdata->job_start_iter + pdata->pipe_cnt); i++)
    {
        ret = AIPU_free_tensor_buffers(pdata->ctx, pdata->test_info[t_iter].jobs[i].buffer.handle);
        if (ret != AIPU_STATUS_SUCCESS)
        {
            AIPU_get_status_msg(ret, &status_msg);
            fprintf(stderr, "[TEST ERROR] %s\n", status_msg);
        }
        fprintf(stdout, "[TEST INFO] buffer freed.\n");
    }

finish:
    return data;
}

void* job_load_scheduling_thread(void* data)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    pipe_job_scheduling_data_t* pdata = (pipe_job_scheduling_data_t*)data;
    const char* status_msg = nullptr;
    uint32_t t_iter = pdata->test_info_iter;

    if ((nullptr == pdata) || (nullptr == pdata->test_info))
    {
        goto finish;
    }

    ret = AIPU_load_graph_helper(pdata->ctx, pdata->test_info[t_iter].bench.graph_fname.c_str(),
        &pdata->test_info[t_iter].gdesc);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_load_graph_helper: %s\n", status_msg);
        goto finish;
    }
    fprintf(stdout, "[TEST INFO] AIPU load graph successfully.\n");

    if (pdata->use_pipeline)
    {
        pipeline_job_scheduling_thread(data);
    }
    else
    {
        non_pipeline_job_scheduling_thread(data);
    }

    ret = AIPU_unload_graph(pdata->ctx, &pdata->test_info[t_iter].gdesc);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_unload_graph: %s\n", status_msg);
        goto finish;
    }
    fprintf(stdout, "[TEST INFO] graph unloaded.\n");

finish:
    return data;
}