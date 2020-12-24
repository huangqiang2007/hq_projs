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
 * @file  test_bench.h
 * @brief AIPU UMD test header file: test benchmark
 */

#ifndef _TEST_BENCH_H_
#define _TEST_BENCH_H_

#include <vector>
#include <string>
#include "cmd_line_parsing.h"

typedef struct test_vector {
    std::vector<std::string> in_fname;
    std::vector<char*> p_inputs;
    std::vector<std::string> check_fname;
    std::vector<char*> p_checks;
} test_vector_t;

typedef struct output_desc {
    uint32_t id;      /* tensor ID */
    uint32_t offset;  /* offset in an output tensor */
    uint32_t size;    /* real size <= tensor size should be checked */
} output_desc_t;

typedef struct test_bench {
    std::string graph_fname;
    std::vector<test_vector_t> vectors;
    std::string odesc_fname;
    char* p_odesc;
    output_desc_t odesc;
} test_bench_t;

int create_test_bench(cmd_opt_t* opt, test_bench_t* test_bench);
int destroy_test_bench(test_bench_t* test_bench);

#endif /* _TEST_BENCH_H_ */