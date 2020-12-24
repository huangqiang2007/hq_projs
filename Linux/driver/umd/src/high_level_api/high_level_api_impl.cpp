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
 * @file  high_level_api_impl.cpp
 * @brief AIPU User Mode Driver (UMD) High Level (HL) API implementation
 */

#include <string>
#include <cstring>
#include <unistd.h>
#include <sys/mman.h>
#include "../high_level_api.h"
#include "utils/helper.h"

Aipu& OpenDevice()
{
    return Aipu::get_aipu();
}

Aipu::Aipu()
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    ret = AIPU_init_ctx(&ctx);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[UMD ERROR] AIPU_init_ctx: %s\n", status_msg);
        ctx = nullptr;
    }
}

void Aipu::CloseDevice()
{
    if (ctx)
    {
        AIPU_deinit_ctx(ctx);
        ctx = nullptr;
    }
}

Graph* Aipu::LoadGraph(char* filename)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    aipu_graph_desc_t gdesc;
    Graph* graph = nullptr;

#if ((defined X86_LINUX) && (X86_LINUX==1))
    aipu_simulation_config_t config;
    config.simulator = "aipu_simulator";
    config.cfg_file_dir = ".";
    config.output_dir = ".";
    config.simulator_opt = NULL;
    ret = AIPU_config_simulation(ctx, &config);
    AIPU_config_simulation(ctx, &config);
#endif

    ret = AIPU_load_graph_helper(ctx, filename, &gdesc);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[UMD ERROR] AIPU_load_graph_helper: %s\n", status_msg);
        return nullptr;
    }

    graph = new Graph(ctx, gdesc);
    return graph;
}

int Graph::GetInputTensorNumber()
{
    return gdesc.inputs.number;
}

int Graph::GetOutputTensorNumber()
{
    return gdesc.outputs.number;
}

int Graph::LoadInputTensor(int i, const void* data)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;

    if ((uint32_t)i >= gdesc.inputs.number) {
        fprintf(stderr, "[UMD ERROR] Invalid input tensor ID too large!\n");
        return -2;
    }

    if (!data)
    {
        fprintf(stderr, "[UMD ERROR] Invalid input data!\n");
        return -3;
    }

    if (!is_buf_alloc)
    {
        ret = AIPU_alloc_tensor_buffers(ctx, &gdesc, &info);
        if (ret != AIPU_STATUS_SUCCESS)
        {
            AIPU_get_status_msg(ret, &status_msg);
            fprintf(stderr, "[UMD ERROR] AIPU_alloc_tensor_buffers: %s\n", status_msg);
            return ret;
        }
        is_buf_alloc = true;
    }

    memcpy(info.inputs.tensors[i].va, data, gdesc.inputs.desc[i].size);
    return 0;
}

int Graph::LoadInputTensor(int i, const char* filename)
{
    int ret = AIPU_STATUS_SUCCESS;
    void* data = nullptr;
    uint32_t data_size = 0;

    ret = umd_mmap_file_helper(filename, &data, &data_size);
    if (ret)
    {
        return ret;
    }

    ret = LoadInputTensor(i, data);

    munmap(data, data_size);
    return ret;
}

int Graph::Run()
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;

    if (job_id)
    {
        AIPU_clean_job(ctx, job_id);
        job_id = 0;
    }

    ret = AIPU_create_job(ctx, &gdesc, info.handle, &job_id);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[UMD ERROR] AIPU_create_job: %s\n", status_msg);
        return ret;
    }

    ret = AIPU_finish_job(ctx, job_id, -1);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[UMD ERROR] AIPU_finish_job: %s\n", status_msg);
    }

    return ret;
}

std::vector< std::vector<int> > Graph::Run(std::vector<std::string>& input_files)
{
    std::vector< std::vector<int> > results;

    for (uint32_t i = 0; (i < info.inputs.number) &&
            (i < (uint32_t)input_files.size()); i++)
    {
        LoadInputTensor(i, input_files[i].c_str());
    }

    Run();

    for (uint32_t i = 0; i < info.outputs.number; i++)
    {
        results.push_back(GetOutputTensor(i));
    }
    return results;
}

std::vector< std::vector<int> > Graph::Run(std::vector< std::vector<int> >& inputs)
{
    std::vector< std::vector<int> > results;

    for (uint32_t i = 0; i < (i < info.inputs.number) &&
            (i < (uint32_t)inputs.size()); i++)
    {
        LoadInputTensor(i, &inputs[i][0]);
    }

    Run();

    for (uint32_t i = 0; i < info.outputs.number; i++)
    {
        results.push_back(GetOutputTensor(i));
    }
    return results;
}

std::vector<int> Graph::GetOutputTensor(int i)
{
    std::vector<int> data;

    if ((uint32_t)i >= info.outputs.number)
    {
        fprintf(stderr, "[UMD ERROR] Invalid output tensor ID too large!\n");
        return data;
    }

    for (uint32_t ch = 0; ch < gdesc.outputs.desc[i].size; ch++)
    {
        data.push_back(*(char*)((unsigned long)info.outputs.tensors[i].va + ch));
    }

    return data;
}

int Graph::UnloadGraph()
{
    if (job_id)
    {
        AIPU_clean_job(ctx, job_id);
        AIPU_free_tensor_buffers(ctx, info.handle);
        job_id = 0;
    }

    return AIPU_unload_graph(ctx, &gdesc);
}
