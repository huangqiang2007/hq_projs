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
  * @file  job_status_poll.cpp
  * @brief AIPU UMD test implementation file: job status poll
  */

#include <stdio.h>
#include "standard_api.h"
#include "job_status_poll.h"

void* job_status_poll(void* data)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    uint32_t job_cnt = 0;
    int32_t time_out = 200;
    const char* status_msg = nullptr;
    job_status_poll_data_t* pdata = (job_status_poll_data_t*)data;
    if (nullptr == pdata)
    {
        goto finish;
    }

    while (*pdata->stop != pdata->stop_cnt)
    {
        ret = AIPU_poll_jobs_status(pdata->ctx, &job_cnt, time_out);
        if (AIPU_STATUS_SUCCESS != ret)
        {
            AIPU_get_status_msg(ret, &status_msg);
            fprintf(stderr, "[TEST ERROR] AIPU_poll_jobs_status: %s\n", status_msg);
            break;
        }
        if (job_cnt != 0)
        {
            fprintf(stdout, "[TEST INFO] number of jobs states of which changes: %u.\n", job_cnt);
        }
    }

finish:
    return data;
}