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
 * @file  aipu.h
 * @brief AIPU User Mode Driver (UMD) aipu operation wrapper header (x86-linux)
 */


#ifndef _AIPU_H_
#define _AIPU_H_

struct profiling_data {
    struct timeval sched_tv;
    struct timeval done_tv;
};

typedef struct job_status_desc {
    uint32_t job_id;
    uint32_t thread_id;
    uint32_t state;
    struct profiling_data pdata;
} job_status_desc_t;

typedef struct aipu_open_info {
    int fd;
    int host_aipu_shm_offset;
} aipu_open_info_t;

#endif /* _AIPU_H_ */
