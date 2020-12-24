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
 * @file  context.h
 * @brief AIPU User Mode Driver (UMD) context module header
 */

#ifndef _CONTEXT_H_
#define _CONTEXT_H_

#include <map>
#include <pthread.h>
#include "standard_api.h"
#include "device_ctrl.h"
#include "graph/graph.h"

namespace AIRT
{
typedef std::map<uint32_t, Graph*> GraphTable;

class MainContext
{
private:
    DeviceCtrl ctrl;
    GraphTable graphs;
    pthread_rwlock_t gt_lock;
    aipu_runtime_config_t rt_cfg;

private:
    static char umd_status_string[][1024];

private:
    aipu_status_t sort_io_tensor(std::vector<io_tensor_desc_t>& tensors) const;
    template<typename sub_section_desc_type>
    aipu_status_t fill_io_tensor_desc_inner(uint32_t reuse_sec_iter,
        uint32_t sub_sec_iter, const sub_section_desc_type& sub_section_load, graph_info_t& info) const;
    template<typename bss_hdr_type, typename static_section_desc_type,
             typename reuse_section_desc_type, typename sub_section_desc_type>
    aipu_status_t parse_bss_section(graph_info_t& info, uint32_t bss_size) const;
    aipu_status_t parse_graph_header(const void* graph, uint32_t size, graph_info_t& info,
        uint32_t& bss_size) const;
    aipu_status_t parse_graph(const void* graph, uint32_t size, graph_info_t& info) const;
    void print_graph_header_info(const bin_hdr_t& header) const;
    void print_parse_result(const graph_info_t& info, const void* graph) const;

private:
    uint32_t create_unique_graph_id_inner() const;
    aipu_status_t create_graph_object(const graph_info_t& info, bool map_flag, Graph** gobj, uint32_t id);
    Graph* get_graph_object(uint32_t id);
    aipu_status_t destroy_graph_object(Graph** gobj);

private:
    bool is_deinit_ok();
    uint32_t get_max_poll_job_cnt();

public:
    aipu_status_t init();
    void force_deinit();
    aipu_status_t deinit();
    aipu_status_t config_simulation(const aipu_simulation_config_t* config);
    aipu_status_t set_runtime_config(const aipu_runtime_config_t* config);
    aipu_status_t load_graph(const void* graph, uint32_t size, bool map_flag, aipu_graph_desc_t* gdesc);
    aipu_status_t unload_graph(const aipu_graph_desc_t* gdesc);
    aipu_status_t alloc_tensor_buffers(const aipu_graph_desc_t* gdesc, aipu_buffer_alloc_info_t* info);
    aipu_status_t free_tensor_buffers(uint32_t handle);
    aipu_status_t create_new_job(const aipu_graph_desc_t* gdesc, uint32_t handle, uint32_t* job_id);
    aipu_status_t flush_job(uint32_t job_id);
    aipu_status_t wait_for_job_end(uint32_t job_id, int32_t time_out, aipu_job_status_t* status);
    aipu_status_t clean_job(uint32_t job_id);
    aipu_status_t set_dump_options(uint32_t job_id, const aipu_dump_option_t* option);
    aipu_status_t get_debug_info(uint32_t job_id, aipu_debug_info_t* info);
    aipu_status_t get_dev_status(uint32_t* value) const;
    aipu_status_t poll_job_status(uint32_t* job_cnt, int32_t time_out);

public:
    static aipu_status_t get_status_msg(aipu_status_t status, const char** msg);

public:
    MainContext(const MainContext& ctx) = delete;
    MainContext& operator=(const MainContext& ctx) = delete;
    ~MainContext();
    MainContext();
};
}

struct ctx_handle {
    uint32_t handle;
};

#endif /* _CONTEXT_H_ */