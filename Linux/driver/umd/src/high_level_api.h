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
 * @file  high_level_api.h
 * @brief AIPU User Mode Driver (UMD) High Level (HL) API header
 * @version 0.2
 */

#ifndef _HIGH_LEVEL_API_H_
#define _HIGH_LEVEL_API_H_

#include <vector>
#include "standard_api.h"

class Graph
{
private:
    aipu_ctx_handle_t* ctx;
    const char* status_msg;
    aipu_graph_desc_t gdesc;
    aipu_buffer_alloc_info_t info;
    uint32_t job_id;
    bool is_buf_alloc;

public:
    int UnloadGraph();

public:
    int GetInputTensorNumber();
    int GetOutputTensorNumber();

public:
    int LoadInputTensor(int i, const void* data);
    int LoadInputTensor(int i, const char* filename);
    int Run();
    std::vector< std::vector<int> > Run(std::vector<std::string>& input_files);
    std::vector< std::vector<int> > Run(std::vector< std::vector<int> >& inputs);
    std::vector<int> GetOutputTensor(int i);

public:
    Graph(aipu_ctx_handle_t* _ctx, aipu_graph_desc_t _gdesc)
    {
        ctx = _ctx;
        gdesc = _gdesc;
        is_buf_alloc = false;
    }
};

class Aipu
{
private:
    aipu_ctx_handle_t* ctx;
    const char* status_msg;

public:
    Graph* LoadGraph(char* filename);
    void CloseDevice();

public:
    static Aipu& get_aipu()
    {
        static Aipu aipu;
        return aipu;
    }
    Aipu(const Aipu& aipu) = delete;
    Aipu& operator=(const Aipu& aipu) = delete;
    ~Aipu(){};

private:
    Aipu();
};

Aipu& OpenDevice();

#endif /* _HIGH_LEVEL_API_H_ */