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
 * @file  graph_test_info.h
 * @brief AIPU UMD test header
 */

#ifndef _GRAPH_TEST_INFO_H_
#define _GRAPH_TEST_INFO_H_

#include <vector>
#include "standard_api.h"
#include "cmd_line_parsing.h"
#include "test_bench.h"

typedef struct test_job_desc {
    uint32_t id;
    aipu_buffer_alloc_info_t buffer;
    aipu_job_status_t status;
} test_job_desc_t;

typedef struct graph_test_info {
    test_bench_t bench;
    aipu_graph_desc_t gdesc;
    uint32_t job_cnt;
    test_job_desc_t* jobs;
    cmd_opt_t opt;
} graph_test_info_t;

graph_test_info_t* create_gtest_info(int argc, char* argv[], const char* test_case,
    uint32_t graph_cnt, uint32_t pipe_cnt);
void destroy_gtest_info(graph_test_info_t* info, uint32_t graph_cnt);
void load_inputs(graph_test_info_t& info, uint32_t iter);
int check_result_pass(const graph_test_info_t& info, uint32_t job_id);

#endif /* _GRAPH_TEST_INFO_H_ */