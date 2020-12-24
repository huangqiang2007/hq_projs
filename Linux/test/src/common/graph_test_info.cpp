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
 * @file  graph_test_info.cpp
 * @brief AIPU UMD test implementation
 */

#include <iostream>
#include "graph_test_info.h"
#include "test_bench.h"
#include "helper.h"

graph_test_info_t* create_gtest_info(int argc, char* argv[], const char* test_case,
    uint32_t graph_cnt, uint32_t pipe_cnt)
{
    graph_test_info_t* info = nullptr;

    if ((graph_cnt == 0) || (pipe_cnt == 0))
    {
        return info;
    }

    info = new graph_test_info_t[graph_cnt];
    for (uint32_t i = 0; i < graph_cnt; i++)
    {
        info[i].jobs = new test_job_desc_t[pipe_cnt];
        info[i].job_cnt = pipe_cnt;
    }

    parsing_cmd_line(argc, argv, &info[0].opt, test_case);
    for (uint32_t i = 0; i < graph_cnt; i++)
    {
        if (create_test_bench(&info[0].opt, &info[i].bench) != 0)
        {
            destroy_gtest_info(info, graph_cnt);
            info = nullptr;
        }
    }

    return info;
}

void destroy_gtest_info(graph_test_info_t* info, uint32_t graph_cnt)
{
    if (nullptr == info)
    {
        return;
    }

    for (uint32_t i = 0; i < graph_cnt; i++)
    {
        delete[] info[i].jobs;
    }
    destroy_test_bench(&info->bench);

    delete[] info;
}

void load_inputs(graph_test_info_t& info, uint32_t iter)
{
    if (iter >= info.job_cnt)
    {
        fprintf(stderr, "[TEST ERROR] load input failed: iter = %u, job_cnt = %u.\n",
            iter, info.job_cnt);
    }

    if (info.bench.vectors[0].p_inputs.size() != info.gdesc.inputs.number)
    {
        fprintf(stderr, "[TEST ERROR] benchmark input data file number %u != input tensor number %u!\n",
            (uint32_t)info.bench.vectors[0].p_inputs.size(), info.gdesc.inputs.number);
        return;
    }

    for (uint32_t i = 0; i < info.jobs[iter].buffer.inputs.number; i++)
    {
        volatile void* dest = info.jobs[iter].buffer.inputs.tensors[i].va;
        for (uint32_t j = 0; j < info.gdesc.inputs.desc[i].size; j++)
        {
            *(volatile char*)((unsigned long)dest + j) =
                *(char*)((unsigned long)info.bench.vectors[0].p_inputs[i] + j);
        }
    }
}

int check_result_pass(const graph_test_info_t& info, uint32_t job_id)
{
    int offset = 0;
    void* check_base = NULL;
    int tot_size = 0;
    volatile char* out_va = nullptr;
    char* check_va = nullptr;
    uint32_t size = 0;
    int ret = 0;
    int pass = 0;
    test_job_desc_t* job = nullptr;

    for (uint32_t i = 0; i < info.gdesc.outputs.number; i++)
    {
        tot_size += info.gdesc.outputs.desc[i].size;
    }

    check_base = info.bench.vectors[0].p_checks[0];

    for (uint32_t i = 0; i < info.job_cnt; i++)
    {
        if (info.jobs[i].id == job_id)
        {
            job = &info.jobs[i];
            break;
        }
    }

    if (nullptr == job)
    {
        return -1;
    }

    for (uint32_t id = 0; id < info.gdesc.outputs.number; id++)
    {
        /* tensor desc id might not be in order */
        for (uint32_t j = 0; j < info.gdesc.outputs.number; j++)
        {
            if (id == job->buffer.outputs.tensors[j].id)
            {
                out_va = (volatile char*)job->buffer.outputs.tensors[j].va;
                check_va = (char*)((unsigned long)check_base + offset);
                size = info.gdesc.outputs.desc[j].size;
                break;
            }
        }
        ret = is_output_correct(out_va, check_va, size);
        if (ret == true)
        {
            fprintf(stderr, "[TEST INFO] Job #%d Test Result Check PASS! (%u/%u)\n", job_id, id + 1,
                info.gdesc.outputs.number);
        }
        else
        {
            pass = -1;
            fprintf(stderr, "[TEST ERROR] Job #%u Test Result Check FAILED! (%u/%u)\n", job_id, id + 1,
                info.gdesc.outputs.number);
        }
        offset += size;
    }

    return pass;
}