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
 * @brief AIPU User Mode Driver (UMD) aipu operation header (arm-linux)
 */

#ifndef _AIPU_H_
#define _AIPU_H_

#include "aipu_ioctl.h"
#include "aipu_capability.h"
#include "aipu_thread_status.h"
#include "aipu_errcode.h"
#include "aipu_buf_req.h"
#include "aipu_job_desc.h"
#include "aipu_job_status.h"
#include "aipu_io_req.h"
#include "aipu_profiling.h"

typedef struct aipu_open_info {
    int fd;
    uint64_t host_aipu_shm_offset;
    struct aipu_cap cap;
} aipu_open_info_t;

#endif /* _AIPU_H_ */