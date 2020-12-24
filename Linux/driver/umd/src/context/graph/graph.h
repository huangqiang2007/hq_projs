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
 * @file  graph.h
 * @brief AIPU User Mode Driver (UMD) graph module header
 */

#ifndef _GRAPH_H_
#define _GRAPH_H_

#include <map>
#include <deque>
#include <pthread.h>
#include "standard_api.h"
#include "context/device_ctrl.h"
#include "graph_def.h"
#include "graph_info.h"
#include "graph_desc_inner.h"
#include "buffer_desc.h"
#include "job_desc.h"

namespace AIRT
{
typedef enum {
    AIPU_MALLOC_STRATEGY_GROUP,
    AIPU_MALLOC_STRATEGY_SEPARATED
} aipu_malloc_strategy_t;

#define CURRENT_AIPU_MALLOC_STRATEGY AIPU_MALLOC_STRATEGY_GROUP

class Graph
{
private:
    DeviceCtrl& ctrl;
    bool map_flag;
    void*    gbin;
    uint32_t gbin_size;
    uint32_t entry;
    uint32_t arch;
    uint32_t hw_version;
    uint32_t hw_config;
    uint32_t asid_flag;
    uint32_t linked_list_flag;

private:
    /**
     * process shared graph descriptor, IO info., an internal DS
     */
    aipu_graph_desc_inner_t gdesc;
    tbuf_alloc_templ_t tbuf_templ;
    std::vector<io_tensor_desc_t> inputs;
    std::vector<io_tensor_desc_t> outputs;
    std::vector<io_tensor_desc_t> inter_dumps;
    std::vector<io_tensor_desc_t> pdata;
    std::vector<io_tensor_desc_t> plog_data;
    std::vector<param_map_load_desc_t> param_map;
    std::vector<dcr_map_load_desc_t> dcr_map;

private:
    /**
     * process shared & thread private buffers of this graph
     */
    pbuf_info_t pbuf;
    std::map<uint32_t, tbuf_info_t*>  tbufs;
    pthread_rwlock_t tbuf_lock;

private:
    /**
     * thread job descriptors of this graph
     */
    std::map<uint32_t, job_desc_t*> jobs;
    std::deque<uint32_t> sched;
    pthread_rwlock_t job_queue_lock;

private:
    tbuf_info_t* get_tbuf_ptr(uint32_t handle);
    job_desc_t*  get_job_ptr(uint32_t job_id);
    volatile void* get_base_va(int sec_type, tbuf_info_t* tbuf);
    uint32_t get_base_pa(int sec_type, tbuf_info_t* tbuf);
    HOST_PA dev2host(DEV_PA pa);
    DEV_PA host2dev(HOST_PA pa);
    void delete_from_sched_queue_inner(uint32_t job_id);

private:
    bool is_job_exist_inner(uint32_t job_id) const;
    bool is_job_built_inner(uint32_t job_id) const;
    bool is_job_scheduled_inner(uint32_t job_id) const;
    bool is_job_end_inner(uint32_t job_id) const;
    bool is_all_jobs_end_inner() const;
    bool is_asid_enabled() const;

private:
    void create_iobuf_info(const tbuf_info_t* tbuf, const std::vector<io_tensor_desc_t>& io_tensor_desc,
        aipu_tensor_buffer_inner_t& iobuf) const;
    void destroy_iobuf_info(aipu_tensor_buffer_inner_t& iobuf);
    void create_tensor_info(const std::vector<io_tensor_desc_t>& desc, aipu_tensor_info_inner_t& info);
    void destroy_tensor_info(aipu_tensor_info_inner_t& info);
    void create_graph_desc(const graph_info_t& info);
    void destroy_graph_desc();

private:
    bool is_timeout(struct timeval sched_time, int32_t time_out) const;
    void dump_job_buffers(const job_desc_t* job, const tbuf_info_t* tbuf, const char* interfix) const;
    void dump_job_mem_map(const job_desc_t* job, const tbuf_info_t* tbuf) const;
    uint32_t create_buf_handle_inner() const;
    uint32_t create_job_id_inner() const;
    void set_timespec(struct timespec* time, struct timeval* curr, uint32_t time_out) const;
    aipu_status_t alloc_group_buffers(const std::vector<section_desc_t>& sections,
            uint32_t dtype, std::vector<buffer_desc_t>& buffers, buffer_desc_t& group);

public:
    static uint32_t handle2graph_id(uint32_t buf_handle);
    static uint32_t job_id2graph_id(uint32_t job_id);

public:
    bool is_unload_ok();
    void get_graph_desc(aipu_graph_desc_t* gdesc_user) const;
    uint32_t get_sched_job_cnt() const;
    void dump_end_job_buffers(uint32_t job_id);
    aipu_status_t is_job_sched(uint32_t job_id);
    bool is_job_end(uint32_t job_id);

public:
    aipu_status_t load(const graph_info_t& info, bool _map_flag);
    aipu_status_t unload();
    aipu_status_t alloc_thread_buffer(aipu_buffer_alloc_info_t* info);
    aipu_status_t free_thread_buffer(uint32_t handle);
    aipu_status_t build_new_job(uint32_t handle, uint32_t* job_id);
    aipu_status_t flush_job(uint32_t job_id);
    aipu_status_t wait_for_job_end_sleep(uint32_t job_id, int32_t time_out, aipu_job_status_t* status);
    aipu_status_t clean_job(uint32_t job_id);
    aipu_status_t set_dump_options(uint32_t job_id, const aipu_dump_option_t* option);
    aipu_status_t get_debug_info(uint32_t job_id, aipu_debug_info_t* info);
    aipu_status_t update_job_status(job_status_desc* status, bool is_wake_up);
    aipu_status_t get_job_status(uint32_t job_id, aipu_job_status_t* status);
    aipu_status_t update_job_status(uint32_t job_id, job_state_t state);

public:
    Graph(uint32_t _id, DeviceCtrl& _ctrl);
    ~Graph();
    Graph(const Graph& graph) = delete;
    Graph& operator=(const Graph& graph) = delete;
};
}

#endif /* _GRAPH_H_ */