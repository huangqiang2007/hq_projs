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
 * @file  dev_op_wrapper.h
 * @brief AIPU User Mode Driver (UMD) device operation wrapper header
 */

#ifndef _DEV_OP_WRAPPER_H_
#define _DEV_OP_WRAPPER_H_

#include <vector>
#include <stdint.h>
#include "aipu.h"

typedef uint64_t DEV_PA;
typedef uint64_t HOST_PA;

typedef struct buffer_desc {
    volatile void* va;
    HOST_PA  pa;        /**< host physical base address */
    uint64_t size;      /**< buffer size */
    uint64_t real_size; /**< real data size (<= buffer size) */
    uint32_t region_id; /**< ID of the region where this buffer locates in */
} buffer_desc_t;

inline void buffer_desc_init(buffer_desc_t* desc)
{
    if (nullptr != desc)
    {
        desc->va = nullptr;
        desc->pa = 0;
        desc->size = 0;
        desc->real_size = 0;
        desc->region_id = 0;
    }
}

/**
 * @brief This API is used to open an AIPU device.
 *
 * @param info  Reference to a memory location allocated by application where UMD stores the
 *              device information returned.
 * @retval 0 if successful
 */
int dev_op_wrapper_open(aipu_open_info_t& info);
/**
 * @brief This API is used to close an opened AIPU device.
 *
 * @param handle Device handle returned by AIPU_LL_open
 *
 * @retval 0 if successful
 */
int dev_op_wrapper_close(int handle);
/**
 * @brief This API is used to capture value of an AIPU external register.
 *
 * @param handle Device handle returned by AIPU_LL_open
 * @param offset Register offset
 * @param value  Pointer to a memory location allocated by application where UMD stores the
 *               register readback value
 *
 * @retval 0 if successful
 */
int dev_op_wrapper_read32(uint32_t handle, uint32_t offset, uint32_t* value);
/**
 * @brief This API is used to write an AIPU external register.
 *
 * @param handle Device handle returned by AIPU_LL_open
 * @param offset Register offset
 * @param value  Value to be write into this register
 *
 * @retval 0 if successful
 */
int dev_op_wrapper_write32(uint32_t handle, uint32_t offset, uint32_t value);
/**
 * @brief This API is used to request to allocate a host-device shared physically contiguous buffer.
 *
 * @param handle        Device handle returned by AIPU_LL_open
 * @param dtype         Data type
 * @param size          Buffer size requested
 * @param align_in_page Address alignment in page (by default 4KB)
 * @param buf           Pointer to a memory location allocated by application where UMD stores the
 *                      successfully allocated buffer info.
 * @region_id           ID of region where the requested buffer is expected to locate in
 *
 * @retval TBD
 */
int dev_op_wrapper_malloc(uint32_t handle, uint32_t dtype, uint32_t size,
        uint32_t align_in_page, buffer_desc_t* buf, uint32_t region_id = 0);
/**
 * @brief This API is used to request to free a buffer allocated by AIPU_LL_malloc.
 *
 * @param handle Device handle returned by AIPU_LL_open
 * @param buf    Buffer descriptor pointer returned by AIPU_LL_malloc
 *
 * @retval 0 if successful
 */
int dev_op_wrapper_free(uint32_t handle, const buffer_desc_t* buf);
/**
 * @brief This API is used to query job status scheduled with the same handle.
 *
 * @param handle          Device handle returned by AIPU_LL_open
 * @param jobs_status     Reference to a jobs status array
 * @param max_cnt         Maximum job count to poll
 * @param time_out        Time out for polling
 * @param poll_single_job Only poll one job (by default is 0 which means not used)
 * @param job_id          Single job ID to poll (by default is 0 which means not used)
 *
 * @retval 0 if successful
 */
int dev_op_wrapper_poll(uint32_t handle, std::vector<job_status_desc>& jobs_status,
    uint32_t max_cnt, uint32_t time_out, bool poll_single_job = 0, uint32_t job_id = 0);

#endif /* _DEV_OP_WRAPPER_H_ */