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
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <iostream>
#include <string.h>
#include <errno.h>
#include "helper.h"

bool is_output_correct(volatile char* src1, char* src2, uint32_t cnt)
{
    for (uint32_t out_chr = 0; out_chr < cnt; out_chr++)
    {
        if (src1[out_chr] != src2[out_chr])
        {
            return false;
        }
    }
    return true;
}

int dump_file_helper(const char* fname, void* src, unsigned int size)
{
    int ret = 0;
    int fd = 0;

    fd = open(fname, O_RDWR | O_CREAT | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd == -1)
    {
        fprintf(stderr, "[TEST ERROR] create bin file %s failed (errno = %d)!", fname, errno);
        ret = -1;
        goto finish;
    }
    if (ftruncate(fd, size) == -1)
    {
        fprintf(stderr, "[TEST ERROR] create bin file %s failed (errno = %d)!", fname, errno);
        ret = -1;
        goto finish;
    }
    ret = write(fd, src, size);
    if (ret != (int)size)
    {
        fprintf(stderr, "[TEST ERROR] write bin file %s failed, need to write 0x%x bytes, \
            successfully write 0x%x bytes (errno = %d)!", fname, size, ret, errno);
        ret = -1;
        goto finish;
    }
    ret = 0;

finish:
    if (fd != -1)
    {
        close(fd);
    }
    return ret;
}

int load_file_helper(const char* fname, char** dest)
{
    int ret = 0;
    int fd = 0;
    struct stat finfo;

    if ((nullptr == fname) || (nullptr == dest))
    {
        return -1;
    }

    *dest = nullptr;

    if (stat(fname, &finfo) != 0)
    {
        fprintf(stderr, "open file failed: %s! (errno = %d)\n", fname, errno);
        return -1;
    }

    fd = open(fname, O_RDONLY);
    if (fd <= 0)
    {
        fprintf(stderr, "open file failed: %s! (errno = %d)\n", fname, errno);
        return -1;
    }

    *dest = new char[finfo.st_size];

    if (read(fd, *dest, finfo.st_size) < 0)
    {
        fprintf(stderr, "load file failed: %s! (errno = %d)\n", fname, errno);
        ret = -1;
        goto finish;
    }

finish:
    if (fd > 0)
    {
        close(fd);
    }
    if ((ret < 0) && (nullptr != dest) && (nullptr != *dest))
    {
        delete[] *dest;
        *dest = nullptr;
    }
    return ret;
}