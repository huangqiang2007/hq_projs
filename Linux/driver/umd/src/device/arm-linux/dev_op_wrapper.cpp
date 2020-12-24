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
 * @file  dev_op_wrapper.cpp
 * @brief AIPU User Mode Driver (UMD) device operation wrapper implementation
 */

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include "device/dev_op_wrapper.h"

int dev_op_wrapper_open(aipu_open_info_t& info)
{
    int ret = 0;
    int fd = 0;
    uint64_t host_aipu_shm_offset = 0;
    aipu_cap cap;

    fd = open("/dev/aipu", O_RDWR | O_SYNC);
    if (fd <= 0)
    {
        goto finish;
    }
    ret = ioctl(fd, IPUIOC_REQSHMMAP, &host_aipu_shm_offset);
    if (ret != 0)
    {
        goto fail;
    }
    ret = ioctl(fd, IPUIOC_QUERYCAP, &cap);
    if (ret != 0)
    {
        goto fail;
    }

    /* success */
    info.fd = fd;
    info.host_aipu_shm_offset = host_aipu_shm_offset;
    info.cap = cap;
    goto finish;

fail:
    close(fd);

finish:
    return ret;
}

int dev_op_wrapper_close(int handle)
{
    return close(handle);
}

int dev_op_wrapper_read32(uint32_t handle, uint32_t offset, uint32_t* value)
{
    int ret = 0;
    aipu_io_req ioreq;
    if (nullptr == value)
    {
        ret = AIPU_ERRCODE_INTERNAL_NULLPTR;
        goto finish;
    }

    ioreq.rw = AIPU_IO_READ;
    ioreq.offset = offset;
    ret = ioctl(handle, IPUIOC_REQIO, &ioreq);
    if ((AIPU_ERRCODE_NO_ERROR != ret) || (AIPU_ERRCODE_NO_ERROR != ioreq.errcode))
    {
        goto finish;
    }

    /* success */
    *value = ioreq.value;

finish:
    return ret;
}

int dev_op_wrapper_write32(uint32_t handle, uint32_t offset, uint32_t value)
{
    int ret = 0;
    aipu_io_req ioreq;

    ioreq.rw = AIPU_IO_WRITE;
    ioreq.offset = offset;
    ioreq.value = value;
    ret = ioctl(handle, IPUIOC_REQIO, &ioreq);
    if ((AIPU_ERRCODE_NO_ERROR != ret) || (AIPU_ERRCODE_NO_ERROR != ioreq.errcode))
    {
        goto finish;
    }

finish:
    return ret;
}

int dev_op_wrapper_malloc(uint32_t handle, uint32_t dtype, uint32_t size,
        uint32_t align_in_page, buffer_desc_t* buf, uint32_t region_id)
{
    int ret = 0;
    buf_request buf_req;
    buf_req.bytes = size;
    buf_req.align_in_page = align_in_page;
    buf_req.data_type = dtype;
    buf_req.region_id = region_id;
    buf_req.alloc_flag = AIPU_ALLOC_FLAG_DEFAULT;
    buf_req.errcode = AIPU_ERRCODE_NO_ERROR;
    void* ptr = nullptr;

    if (nullptr == buf)
    {
        ret = AIPU_ERRCODE_INTERNAL_NULLPTR;
        goto finish;
    }

    if (0 == size)
    {
        ret = AIPU_ERRCODE_NO_MEMORY;
        goto finish;
    }

    ret = ioctl(handle, IPUIOC_REQBUF, &buf_req);
    if ((ret != 0) || (buf_req.errcode != AIPU_ERRCODE_NO_ERROR))
    {
        ret = buf_req.errcode;
        goto finish;
    }

    ptr = mmap(NULL, buf_req.desc.bytes, PROT_READ | PROT_WRITE, MAP_SHARED,
        handle, buf_req.desc.dev_offset);
    if (ptr == MAP_FAILED)
    {
        ret = -1;
        goto finish;
    }

    /* success */
    buf->pa = buf_req.desc.pa;
    buf->va = ptr;
    buf->size = buf_req.desc.bytes;
    buf->region_id = buf_req.desc.region_id;
    buf->real_size = buf_req.bytes;

finish:
    return ret;
}

int dev_op_wrapper_free(uint32_t handle, const buffer_desc_t* buf)
{
    int ret = 0;
    buf_desc desc;

    if (nullptr == buf)
    {
        ret = AIPU_ERRCODE_INTERNAL_NULLPTR;
        goto finish;
    }

    desc.pa = buf->pa;
    desc.bytes = buf->size;
    munmap((void*)buf->va, buf->size);
    ret = ioctl(handle, IPUIOC_FREEBUF, &desc);

finish:
    return ret;
}

int dev_op_wrapper_poll(uint32_t handle, std::vector<job_status_desc>& jobs_status,
    uint32_t max_cnt, uint32_t time_out, bool poll_single_job, uint32_t job_id)
{
    int ret = 0;
    struct pollfd poll_list;
    job_status_query status_query;

    poll_list.fd = handle;
    poll_list.events = POLLIN | POLLPRI;

    if (max_cnt == 0)
    {
        return -2;
    }

    ret = poll(&poll_list, 1, time_out);
    if (ret < 0)
    {
        printf("[UMD ERROR] poll failed!\n");
        goto finish;
    }

    if ((poll_list.revents & POLLIN) == POLLIN)
    {
        status_query.get_single_job = poll_single_job;
        status_query.job_id = job_id;
        status_query.max_cnt = max_cnt;
        status_query.status = new job_status_desc[max_cnt];
        status_query.errcode = AIPU_ERRCODE_NO_ERROR;
        ret = ioctl(handle, IPUIOC_QUERYSTATUS, &status_query);
        if ((0 != ret) || (status_query.errcode != AIPU_ERRCODE_NO_ERROR))
        {
            goto clean;
        }
    }
    else
    {
        goto finish;
    }

    for (uint32_t i = 0; i < status_query.poll_cnt; i++)
    {
        jobs_status.push_back(status_query.status[i]);
    }

clean:
    delete[] status_query.status;

finish:
    return ret;
}