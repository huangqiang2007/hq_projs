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
 * @file  test_bench.cpp
 * @brief AIPU UMD test implementation file: test benchmark creation
 */

#include <string.h>
#include "test_bench.h"
#include "helper.h"

int create_test_bench(cmd_opt_t* opt, test_bench_t* test_bench)
{
    int ret = 0;
    test_vector_t test_vector;
    char* temp = nullptr;
    char* dest = nullptr;

    if ((nullptr == opt) || (nullptr == test_bench))
    {
        ret = -1;
        goto finish;
    }

    test_bench->graph_fname = opt->bin_file_name;

    /* single test vector */
    test_bench->vectors.push_back(test_vector);

    temp = strtok(opt->data_file_name, ",");
    test_bench->vectors[0].in_fname.push_back(temp);
    while (temp)
    {
        temp = strtok(nullptr, ",");
        if (temp != nullptr)
        {
            test_bench->vectors[0].in_fname.push_back(temp);
        }
    }

    for (uint32_t i = 0; i < test_bench->vectors[0].in_fname.size(); i++)
    {
        ret = load_file_helper(test_bench->vectors[0].in_fname[i].c_str(), &dest);
        if (ret != 0)
        {
            goto finish;
        }
        test_bench->vectors[0].p_inputs.push_back(dest);
    }

    /* output check tensors are stored in one output file */
    test_bench->vectors[0].check_fname.push_back(opt->check_file_name);
    ret = load_file_helper(test_bench->vectors[0].check_fname[0].c_str(), &dest);
    if (ret != 0)
    {
        goto finish;
    }
    test_bench->vectors[0].p_checks.push_back(dest);

    /* reserved */
    test_bench->p_odesc = nullptr;

finish:
    if (ret != 0)
    {
        destroy_test_bench(test_bench);
    }
    return ret;
}

int destroy_test_bench(test_bench_t* test_bench)
{
    int ret = 0;

    if (nullptr == test_bench)
    {
        return -1;
    }

    for (uint32_t i = 0; i < test_bench->vectors[0].p_inputs.size(); i++)
    {
        delete[] test_bench->vectors[0].p_inputs[i];
        test_bench->vectors[0].p_inputs[i] = nullptr;
    }
    for (uint32_t i = 0; i < test_bench->vectors[0].p_checks.size(); i++)
    {
        delete[] test_bench->vectors[0].p_checks[i];
        test_bench->vectors[0].p_checks[i] = nullptr;
    }

    test_bench->vectors[0].p_inputs.clear();
    test_bench->vectors[0].p_checks.clear();
    test_bench->vectors[0].in_fname.clear();
    test_bench->vectors[0].check_fname.clear();
    return ret;
}