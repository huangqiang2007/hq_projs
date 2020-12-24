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
  * @file  scheduling_thread.h
  * @brief AIPU UMD test header file: job scheduling thread
  */

#ifndef _JOB_SCHEDULING_THREAD_H_
#define _JOB_SCHEDULING_THREAD_H_

#include <atomic>
#include "standard_api.h"
#include "common/common.h"

typedef struct pipe_job_scheduling_data {
    aipu_ctx_handle_t* ctx;
    graph_test_info_t* test_info;
    uint32_t test_info_iter;
    uint32_t pipe_cnt;
    uint32_t job_start_iter;
    uint32_t loop_cnt;
    bool use_pipeline;
    std::atomic<uint32_t>* stop;
} pipe_job_scheduling_data_t;

void* pipeline_job_scheduling_thread(void* data);

void* non_pipeline_job_scheduling_thread(void* data);

void* job_load_scheduling_thread(void* data);

#endif /* _JOB_SCHEDULING_THREAD_H_ */