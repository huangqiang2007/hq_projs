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
 * @file  low_level_api_impl.cpp
 * @brief AIPU User Mode Driver (UMD) Low Level API implementation file
 */

#include "low_level_api.h"
#include "device/dev_op_wrapper.h"
#include "low_level_ctx.h"

static ll_ctx_t ctx;

aipu_ll_status_t AIPU_LL_open(AIPU_HANDLE* handle)
{
    aipu_ll_status_t ret = AIPU_LL_STATUS_SUCCESS;

    if (nullptr == handle)
    {
        ret = AIPU_LL_STATUS_ERROR_INVALID_PTR;
        goto finish;
    }

    if (dev_op_wrapper_open(ctx.info) != 0)
    {
        ret = AIPU_LL_STATUS_ERROR_OPEN_FAILED;
        goto finish;
    }

    /* success */
    *handle = ctx.info.fd;

finish:
    return ret;
}

aipu_ll_status_t AIPU_LL_close(AIPU_HANDLE handle)
{
    aipu_ll_status_t ret = AIPU_LL_STATUS_SUCCESS;

    if (dev_op_wrapper_close(handle) != 0)
    {
        ret = AIPU_LL_STATUS_ERROR_INVALID_HANDLE;
    }
    else
    {
        ctx.info.fd = 0;
    }

    return ret;
}

aipu_ll_status_t AIPU_LL_get_hw_version(AIPU_HANDLE handle, uint32_t* version)
{
    aipu_ll_status_t ret = AIPU_LL_STATUS_SUCCESS;

    if (nullptr == version)
    {
        ret = AIPU_LL_STATUS_ERROR_INVALID_PTR;
        goto finish;
    }

    if ((handle == 0) || (ctx.info.fd != handle))
    {
        ret = AIPU_LL_STATUS_ERROR_INVALID_HANDLE;
        goto finish;
    }

    /* success */
    *version = ctx.info.cap.isa_version;

finish:
    return ret;
}

aipu_ll_status_t AIPU_LL_read_reg32(AIPU_HANDLE handle, uint32_t offset, uint32_t* value)
{
    aipu_ll_status_t ret = AIPU_LL_STATUS_SUCCESS;
    int kern_ret = 0;

    if (nullptr == value)
    {
        ret = AIPU_LL_STATUS_ERROR_INVALID_PTR;
        goto finish;
    }

    if ((handle == 0) || (ctx.info.fd != handle))
    {
        ret = AIPU_LL_STATUS_ERROR_INVALID_HANDLE;
        goto finish;
    }

    kern_ret = dev_op_wrapper_read32(handle, offset, value);
    if (kern_ret != AIPU_ERRCODE_NO_ERROR)
    {
        ret = AIPU_LL_STATUS_ERROR_READ_REG_FAIL;
    }

finish:
    return ret;
}

aipu_ll_status_t AIPU_LL_write_reg32(AIPU_HANDLE handle, uint32_t offset, uint32_t value)
{
    aipu_ll_status_t ret = AIPU_LL_STATUS_SUCCESS;
    int kern_ret = 0;

    if ((handle == 0) || (ctx.info.fd != handle))
    {
        ret = AIPU_LL_STATUS_ERROR_INVALID_HANDLE;
        goto finish;
    }

    kern_ret = dev_op_wrapper_write32(handle, offset, value);
    if (kern_ret != AIPU_ERRCODE_NO_ERROR)
    {
        ret = AIPU_LL_STATUS_ERROR_WRITE_REG_FAIL;
    }

finish:
    return ret;
}

aipu_ll_status_t AIPU_LL_malloc(AIPU_HANDLE handle, uint32_t size, uint32_t align,
    aipu_buffer_t* buf)
{
    aipu_ll_status_t ret = AIPU_LL_STATUS_SUCCESS;
    buffer_desc_t desc;

    if (0 == size)
    {
        ret = AIPU_LL_STATUS_ERROR_INVALID_SIZE;
        goto finish;
    }

    if (nullptr == buf)
    {
        ret = AIPU_LL_STATUS_ERROR_INVALID_PTR;
        goto finish;
    }

    if ((handle == 0) || (ctx.info.fd != handle))
    {
        ret = AIPU_LL_STATUS_ERROR_INVALID_HANDLE;
        goto finish;
    }

    if (dev_op_wrapper_malloc(handle, AIPU_MM_DATA_TYPE_NONE, size, align, &desc) != 0)
    {
        ret = AIPU_LL_STATUS_ERROR_MALLOC_FAIL;
        goto finish;
    }

    /* success */
    buf->pa = desc.pa - ctx.info.host_aipu_shm_offset;
    buf->va = (void*)desc.va;
    buf->size = desc.size;

finish:
    return ret;
}

aipu_ll_status_t AIPU_LL_free(AIPU_HANDLE handle, aipu_buffer_t* buf)
{
    aipu_ll_status_t ret = AIPU_LL_STATUS_SUCCESS;
    buffer_desc_t desc;

    if (nullptr == buf)
    {
        ret = AIPU_LL_STATUS_ERROR_INVALID_PTR;
        goto finish;
    }

    if ((0 == buf->size) || (nullptr == buf->va))
    {
        ret = AIPU_LL_STATUS_ERROR_INVALID_PA;
        goto finish;
    }

    if ((handle == 0) || (ctx.info.fd != handle))
    {
        ret = AIPU_LL_STATUS_ERROR_INVALID_HANDLE;
        goto finish;
    }

    desc.pa = buf->pa + ctx.info.host_aipu_shm_offset;
    desc.va = buf->va;
    desc.size = buf->size;
    if (dev_op_wrapper_free(handle, &desc) != 0)
    {
        ret = AIPU_LL_STATUS_ERROR_FREE_FAIL;
    }

finish:
    return ret;
}
