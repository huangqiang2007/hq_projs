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
  * @file  job_status_poll.h
  * @brief AIPU UMD test header file: job status poll
  */

#ifndef _JOB_STATUS_POLL_H_
#define _JOB_STATUS_POLL_H_

#include <atomic>
#include "standard_api.h"

typedef struct job_status_poll_data {
    std::atomic<uint32_t>* stop;
    aipu_ctx_handle_t* ctx;
    uint32_t stop_cnt;
} job_status_poll_data_t;

void* job_status_poll(void* data);

#endif /* _JOB_STATUS_POLL_H_ */