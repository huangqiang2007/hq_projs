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
 * @file  job_desc.h
 * @brief AIPU User Mode Driver (UMD) job descriptor module header
 */

#ifndef _JOB_DESC_H_
#define _JOB_DESC_H_

#include <string>
#include <pthread.h>
#include "standard_api.h"
#include "device/dev_op_wrapper.h"
#include "buffer_desc.h"

typedef enum {
    JOB_STATE_NO_STATE = 0,
    JOB_STATE_DONE,
    JOB_STATE_EXCEPTION,
    JOB_STATE_BUILT,
    JOB_STATE_SCHED,
    JOB_STATE_TIMEOUT
} job_state_t;

typedef struct aipu_code_section_info {
    DEV_PA instruction_base_pa;
    DEV_PA start_pc_pa;
    DEV_PA interrupt_pc_pa;
} aipu_code_section_info_t;

typedef struct dev_config {
    uint32_t arch;
    uint32_t hw_version;
    uint32_t hw_config;
    aipu_code_section_info_t code;
    uint32_t code_size;
    DEV_PA rodata_base;
    uint32_t rodata_size;
    DEV_PA stack_base;
    uint32_t stack_size;
    DEV_PA static_base;
    uint32_t static_size;
    DEV_PA reuse_base;
    uint32_t reuse_size;
    int enable_prof;
    uint32_t enable_asid;
} dev_config_t;

typedef struct job_desc {
    uint32_t id;
    uint32_t buf_handle;
    job_state_t state;
    dev_config_t config;
    uint32_t dump_flag;
    std::string dump_fname_suffix;
    std::string dump_dir;
    struct profiling_data pdata;
    struct timeval timeout_start;
    pthread_mutex_t lock;
    pthread_cond_t cond;
} job_desc_t;

#endif /* _JOB_DESC_H_ */