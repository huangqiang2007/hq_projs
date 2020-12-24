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
 * @file  main.cpp
 * @brief AIPU SDK Driver Out-of-box Example
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <getopt.h>
#include <sys/mman.h>
#include <iostream>
#include <string>
#include <cstring>
#include <errno.h>
#include <vector>
#include "standard_api.h"

static struct option opts[] = {
    { "sim", required_argument, NULL, 's' },
    { "cfg_dir", required_argument, NULL, 'C' },
    { "bin", required_argument, NULL, 'b' },
    { "idata", required_argument, NULL, 'i' },
    { "check", required_argument, NULL, 'c' },
    { "dump_dir", required_argument, NULL, 'd' },
    { NULL, 0, NULL, 0 }
};

#define FNAME_MAX_LEN 4096

using namespace std;

static void* load_data_from_file(const char* fname, int* size)
{
    int fd = 0;
    struct stat finfo;
    void* start = nullptr;

    if ((nullptr == fname) || (nullptr == size))
    {
        goto finish;
    }

    if (stat(fname, &finfo) != 0)
    {
        goto finish;
    }

    fd = open(fname, O_RDONLY);
    if (fd <= 0)
    {
        fprintf(stderr, "open file failed: %s! (errno = %d)\n", fname, errno);
        goto finish;
    }

    start = mmap(nullptr, finfo.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (MAP_FAILED == start)
    {
        fprintf(stderr, "failed in mapping graph file: %s! (errno = %d)\n", fname, errno);
        goto finish;
    }

    /* success */
    *size = finfo.st_size;

finish:
    if (fd > 0)
    {
        close(fd);
    }
    return start;
}

int main(int argc, char* argv[])
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    int pass = 0;

    extern char *optarg;
    int opt_idx = 0;
    int c = 0;
    char simulator[FNAME_MAX_LEN] = { 0 };
    char cfg_file_dir[FNAME_MAX_LEN] = { 0 };
    char bin_file_name[FNAME_MAX_LEN] = { 0 };
    char data_file_name[FNAME_MAX_LEN] = { 0 };
    char check_file_name[FNAME_MAX_LEN] = { 0 };
    char dump_dir[FNAME_MAX_LEN] = { 0 };

    aipu_ctx_handle_t* ctx = nullptr;
    const char* status_msg = nullptr;
    aipu_simulation_config_t config;
    aipu_graph_desc_t gdesc;
    aipu_buffer_alloc_info_t buffer;
    uint32_t job_id = 0;
    int32_t time_out = -1;

    void* in_data = nullptr;
    int in_fsize = 0;
    void* check_data = nullptr;
    int check_fsize = 0;

    if (argc < 3)
    {
        fprintf(stderr, "[TEST ERROR] need more options!\n");
        goto finish;
    }

    while (1)
    {
        c = getopt_long (argc, argv, "s:C:b:i:c:d:", opts, &opt_idx);
        if (-1 == c)
        {
            break;
        }

        switch (c)
        {
        case 0:
            if (opts[opt_idx].flag != 0)
            {
                break;
            }
            fprintf (stdout, "option %s", opts[opt_idx].name);
            if (optarg)
            {
                printf (" with arg %s", optarg);
            }
            printf ("\n");
            break;

        case 's':
            strcpy(simulator, optarg);
            break;

        case 'C':
            strcpy(cfg_file_dir, optarg);
            break;

        case 'b':
            strcpy(bin_file_name, optarg);
            break;

        case 'i':
            strcpy(data_file_name, optarg);
            break;

        case 'c':
            strcpy(check_file_name, optarg);
            break;

        case 'd':
            strcpy(dump_dir, optarg);
            break;

        case '?':
            break;

        default:
            break;
        }
    }

    if ((simulator[0] == 0) || (cfg_file_dir[0] == 0) || (bin_file_name[0] == 0) ||
        (data_file_name[0] == 0) || (check_file_name[0] == 0) || (dump_dir[0] == 0))
    {
        fprintf(stderr, "[TEST ERROR] need more options!\n");
        goto finish;
    }

    ret = AIPU_init_ctx(&ctx);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_init_ctx: %s\n", status_msg);
        goto finish;
    }

    config.simulator = simulator;
    config.cfg_file_dir = cfg_file_dir;
    config.output_dir = dump_dir;
    config.simulator_opt = NULL;
    ret = AIPU_config_simulation(ctx, &config);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_config_simulation: %s\n", status_msg);
        goto finish;
    }

    ret = AIPU_load_graph_helper(ctx, bin_file_name, &gdesc);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_load_graph_helper: %s\n", status_msg);
        goto finish;
    }
    fprintf(stdout, "[TEST INFO] AIPU load graph successfully.\n");

    ret = AIPU_alloc_tensor_buffers(ctx, &gdesc, &buffer);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_alloc_tensor_buffers: %s\n", status_msg);
        goto clean_graph;
    }

    /* loading graph input data... */
    /* this test only support single input graph */
    /* for multi-input graphs, just load as this one by one */
    in_data = load_data_from_file(data_file_name, &in_fsize);
    if (!in_data)
    {
        fprintf(stdout, "[TEST ERROR] Load input from file failed!\n");
        goto finish;
    }
    memcpy(buffer.inputs.tensors[0].va, in_data, buffer.inputs.tensors[0].size);

    ret = AIPU_create_job(ctx, &gdesc, buffer.handle, &job_id);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_create_job: %s\n", status_msg);
        goto clean_buffer;
    }

    ret = AIPU_finish_job(ctx, job_id, time_out);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_finish_job: %s\n", status_msg);
        pass = -1;
        goto clean_job;
    }

    /* loading output check data... */
    /* this test only support single output graph */
    /* for multi-output graphs, just load as this one by one */
    check_data = load_data_from_file(check_file_name, &check_fsize);
    if (!check_data)
    {
        fprintf(stdout, "[TEST ERROR] Load input from file failed!\n");
        goto finish;
    }

    /* checking output data correctness... */
    for (uint32_t i = 0; i < buffer.outputs.tensors[0].size; i++)
    {
        if (*(char*)((unsigned long)buffer.outputs.tensors[0].va + i) !=
            *(char*)((unsigned long)check_data + i))
        {
            pass = -1;
            break;
        }
    }
    if (pass != 0)
    {
        fprintf(stderr, "[TEST ERROR] Test Result Check FAILED!\n");
    }
    else
    {
        fprintf(stdout, "[TEST INFO] Test Result Check PASS!\n");
    }

clean_job:
    ret = AIPU_clean_job(ctx, job_id);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_clean_job: %s\n", status_msg);
        goto finish;
    }

clean_buffer:
    ret = AIPU_free_tensor_buffers(ctx, buffer.handle);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_free_tensor_buffers: %s\n", status_msg);
        goto finish;
    }

clean_graph:
    ret = AIPU_unload_graph(ctx, &gdesc);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_unload_graph; %s\n", status_msg);
        goto finish;
    }

    ret = AIPU_deinit_ctx(ctx);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        AIPU_get_status_msg(ret, &status_msg);
        fprintf(stderr, "[TEST ERROR] AIPU_deinit_ctx: %s\n", status_msg);
        goto finish;
    }

finish:
    if (AIPU_STATUS_SUCCESS != ret)
    {
        pass = -1;
    }
    if (in_data)
    {
        munmap(in_data, in_fsize);
    }
    if (check_data)
    {
        munmap(check_data, check_fsize);
    }
    return pass;
}