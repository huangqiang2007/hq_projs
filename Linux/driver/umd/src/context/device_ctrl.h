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
 * @file  device_ctrl.h
 * @brief AIPU User Mode Driver (UMD) device control module header
 */

#ifndef _DEVICE_CTRL_H_
#define _DEVICE_CTRL_H_

#include <stdlib.h>
#include <stdio.h>
#include <map>
#include <vector>
#include <string>
#include <pthread.h>
#include <signal.h>
#include "standard_api.h"
#include "device/dev_op_wrapper.h"
#include "graph/graph_info.h"
#include "graph/buffer_desc.h"
#include "graph/job_desc.h"
#include "arch/aipu_arch.h"

#define FNAME_LEN 2048
#define OPT_LEN   2148
#define CMD_MEN   8000

namespace AIRT
{
#if (defined X86_LINUX) && (X86_LINUX==1)
typedef struct output_desc {
    char fname[FNAME_LEN];
    uint32_t id;
    uint32_t size;
    void* va;
    uint32_t pa;
} output_file_desc_t;

typedef enum {
    SIM_FILE_TYPE_CODE,
    SIM_FILE_TYPE_RODATA,
    SIM_FILE_TYPE_DATA,
    SIM_FILE_TYPE_OUTPUT,
} simfile_type_t;

typedef struct simulation {
    uint32_t graph_id;
    char code_fname[FNAME_LEN];
    char rodata_fname[FNAME_LEN];
    char data_fname[FNAME_LEN];
    buffer_desc_t rodata;
    buffer_desc_t data;
    std::vector<output_file_desc_t> outputs;
    char odata_fname[FNAME_LEN];
    uint32_t odata_whole_pa;
    void*    odata_whole_va;
    uint32_t odata_whole_size;
} simulation_res_t;
#endif /* !X86_LINUX */

class DeviceCtrl
{
private:
    int fd;
    uint64_t host_aipu_shm_offset;

private:
#if (defined ARM_LINUX) && (ARM_LINUX==1)
    uint32_t aipu_arch;
    uint32_t aipu_version;
    uint32_t aipu_hw_config;
#endif /* !ARM_LINUX */

#if (defined X86_LINUX) && (X86_LINUX==1)
    /* used ony for simulation */
    std::string simulator;
    std::string cfg_file_dir;
    std::string output_dir;
    std::string additional_opt;
    bool has_additional_opt;
    uint32_t simulation_malloc_top;
    char simulation_cmd[CMD_MEN];
    std::map<uint32_t, simulation_res_t> graphs;
    std::map<uint32_t, aipu_arch_t> aipu_arch;
    pthread_mutex_t glock;
    int use_new_arch_sim;

private:
    aipu_status_t create_simulation_input_file(uint32_t graph_id, const buffer_desc_t& desc,
        char* fname, const char* interfix, uint32_t load_size);
    uint32_t get_sim_data_size(const pbuf_alloc_templ_t& pbuf_templ,
        const tbuf_alloc_templ_t& tbuf_templ) const;
    aipu_status_t update_z1_simulation_sys_cfg(uint32_t arch, FILE *fp) const;
    aipu_status_t update_simulation_rtcfg_z1_old_arch(simulation_res_t& sim, const dev_config_t& config,
        FILE *fp, char* cfg_fname);
    aipu_status_t update_simulation_rtcfg_new_arch(simulation_res_t& sim, const dev_config_t& config,
        FILE *fp, char* cfg_fname);
    aipu_status_t update_simulation_rtcfg(uint32_t graph_id, const dev_config_t& config);
    void init_aipu_arch();

public:
    aipu_status_t config_simulation(const aipu_simulation_config_t* config);
    aipu_status_t simulation_alloc_data_buffer(uint32_t graph_id, const pbuf_alloc_templ_t& pbuf_templ,
        pbuf_info_t& pbuf, const tbuf_alloc_templ_t& tbuf_templ, tbuf_info_t& tbuf);
    aipu_status_t simulation_free_data_buffer(uint32_t graph_id);
    aipu_status_t simulation_set_io_info(uint32_t graph_id, const iobuf_info_t& iobuf);
#endif /* !X86_LINUX */

public:
    aipu_status_t init();
    aipu_status_t deinit();
    bool match_target_dev(uint32_t arch, uint32_t version, uint32_t hw_config) const;
    void unload_graph(uint32_t graph_id);
    aipu_status_t malloc_buf(uint32_t dtype, uint32_t size, uint32_t align, buffer_desc_t* buf,
            uint32_t region_id = 0);
    void load_buffer(volatile void* dest, const void* src, uint32_t bytes);
    aipu_status_t free_buf(const buffer_desc_t* buf);
    aipu_status_t alloc_text_buffer(uint32_t graph_id, const pbuf_alloc_templ_t& pbuf_templ,
        buffer_desc_t& ibuf_desc);
    aipu_status_t alloc_rodata_buffer(uint32_t region_id, const tbuf_alloc_templ_t& pbuf_templ,
        buffer_desc_t& rodata, buffer_desc_t& descriptor);
    aipu_status_t free_text_buffer(buffer_desc_t& ibuf_desc);
    aipu_status_t load_text_buffer(uint32_t graph_id, const void* src, uint32_t size,
        buffer_desc_t& ibuf_desc);
    uint64_t get_shm_offset() const;
    aipu_status_t schedule_job_on_aipu(uint32_t graph_id, job_desc_t *job);
    aipu_status_t get_dev_status(uint32_t *value) const;
    aipu_status_t kill_timeout_job(uint32_t job_id);

public:
    aipu_status_t poll_status(std::vector<job_status_desc>& jobs_status, uint32_t max_cnt,
        uint32_t time_out, bool poll_single_job = 0, uint32_t job_id = 0);

public:
    DeviceCtrl();
    ~DeviceCtrl();
    DeviceCtrl(const DeviceCtrl& ctrl) = delete;
    DeviceCtrl& operator=(const DeviceCtrl& ctrl) = delete;
};
}

uint32_t get_aligned_addr(uint32_t base, uint32_t align_in_page);

#endif //_DEVICE_CTRL_H_
