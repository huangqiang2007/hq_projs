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
 * @file  graph.cpp
 * @brief AIPU User Mode Driver (UMD) graph module implementation
 */

#include <iostream>
#include <fstream>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/time.h>
#include "graph.h"
#include "utils/helper.h"
#include "utils/log.h"

AIRT::Graph::Graph(uint32_t _id, DeviceCtrl& _ctrl): ctrl(_ctrl)
{
    gdesc.id = _id;
    map_flag = false;
    gbin = nullptr;
    gbin_size = 0;
    entry = 0;
    asid_flag = 0;
    pthread_rwlock_init(&tbuf_lock, NULL);
    pthread_rwlock_init(&job_queue_lock, NULL);

    buffer_desc_init(&pbuf.text);
    buffer_desc_init(&pbuf.static_group);
    tbufs.clear();
    jobs.clear();
    sched.clear();
    inputs.clear();
    outputs.clear();
    inter_dumps.clear();
    pdata.clear();
    plog_data.clear();
    param_map.clear();
    dcr_map.clear();
}

AIRT::Graph::~Graph()
{
    pthread_rwlock_destroy(&tbuf_lock);
    pthread_rwlock_destroy(&job_queue_lock);
}

uint32_t AIRT::Graph::handle2graph_id(uint32_t buf_handle)
{
    return buf_handle>>16;
}

uint32_t AIRT::Graph::job_id2graph_id(uint32_t job_id)
{
    return job_id>>16;
}

tbuf_info_t* AIRT::Graph::get_tbuf_ptr(uint32_t handle)
{
    tbuf_info_t* tbuf = nullptr;
    std::map<uint32_t, tbuf_info_t*>::iterator tbuf_iter;
    pthread_rwlock_rdlock(&tbuf_lock);
    tbuf_iter = tbufs.find(handle);
    if (tbuf_iter != tbufs.end())
    {
        tbuf = tbuf_iter->second;
    }
    pthread_rwlock_unlock(&tbuf_lock);
    return tbuf;
}

job_desc_t* AIRT::Graph::get_job_ptr(uint32_t job_id)
{
    job_desc_t* job = nullptr;
    std::map<uint32_t, job_desc_t*>::const_iterator job_iter;
    pthread_rwlock_rdlock(&job_queue_lock);
    job_iter = jobs.find(job_id);
    if (job_iter != jobs.end())
    {
        job = job_iter->second;
    }
    pthread_rwlock_unlock(&job_queue_lock);
    return job;
}

volatile void* AIRT::Graph::get_base_va(int sec_type, tbuf_info_t* tbuf)
{
    volatile void* va = nullptr;

    if (sec_type == SECTION_TYPE_RODATA)
    {
        if (tbuf != nullptr)
        {
            va = tbuf->rodata.va;
        }
    }
    else if (sec_type == SECTION_TYPE_DESCRIPTOR)
    {
        if (tbuf != nullptr)
        {
            va = tbuf->descriptor.va;
        }
    }
    else if (sec_type == SECTION_TYPE_TEXT)
    {
        va = pbuf.text.va;
    }

    return va;
}

uint32_t AIRT::Graph::get_base_pa(int sec_type, tbuf_info_t* tbuf)
{
    uint32_t pa = 0;

    if (sec_type == SECTION_TYPE_RODATA)
    {
        if (tbuf != nullptr)
        {
            pa = tbuf->rodata.pa;
        }
    }
    else if (sec_type == SECTION_TYPE_DESCRIPTOR)
    {
        if (tbuf != nullptr)
        {
            pa = tbuf->descriptor.pa;
        }
    }
    else if (sec_type == SECTION_TYPE_TEXT)
    {
        pa = pbuf.text.pa;
    }

    return pa;
}

HOST_PA AIRT::Graph::dev2host(DEV_PA pa)
{
    return (HOST_PA)(pa + ctrl.get_shm_offset());
}

DEV_PA AIRT::Graph::host2dev(HOST_PA pa)
{
    return (DEV_PA)(pa - ctrl.get_shm_offset());
}

void AIRT::Graph::delete_from_sched_queue_inner(uint32_t job_id)
{
    std::deque<uint32_t>::iterator sched_iter;
    for (sched_iter = sched.begin(); sched_iter != sched.end(); sched_iter++)
    {
        if (*sched_iter == job_id)
        {
            sched.erase(sched_iter);
            break;
        }
    }
}

bool AIRT::Graph::is_job_exist_inner(uint32_t job_id) const
{
    return (is_job_built_inner(job_id) || is_job_scheduled_inner(job_id) || is_job_end_inner(job_id));
}

bool AIRT::Graph::is_job_built_inner(uint32_t job_id) const
{
    bool ret = false;
    std::map<uint32_t, job_desc_t*>::const_iterator job_iter = jobs.find(job_id);
    if (job_iter != jobs.end())
    {
        ret = (job_iter->second->state == JOB_STATE_BUILT);
    }
    return ret;
}

bool AIRT::Graph::is_job_scheduled_inner(uint32_t job_id) const
{
    bool ret = false;
    std::map<uint32_t, job_desc_t*>::const_iterator job_iter = jobs.find(job_id);
    if (job_iter != jobs.end())
    {
        ret = (job_iter->second->state == JOB_STATE_SCHED);
    }
    return ret;
}

bool AIRT::Graph::is_job_end_inner(uint32_t job_id) const
{
    bool ret = false;
    std::map<uint32_t, job_desc_t*>::const_iterator job_iter = jobs.find(job_id);
    if (job_iter != jobs.end())
    {
        ret = (job_iter->second->state == JOB_STATE_DONE) ||
            (job_iter->second->state == JOB_STATE_EXCEPTION);
    }
    return ret;
}

bool AIRT::Graph::is_all_jobs_end_inner() const
{
    std::map<uint32_t, job_desc_t*>::const_iterator job_iter;
    for (job_iter = jobs.begin(); job_iter != jobs.end(); job_iter++)
    {
        if ((job_iter->second->state != JOB_STATE_DONE) &&
            (job_iter->second->state != JOB_STATE_EXCEPTION))
        {
            return false;
        }
    }
    return true;
}

uint32_t AIRT::Graph::get_sched_job_cnt() const
{
    return sched.size();
}

void AIRT::Graph::create_iobuf_info(const tbuf_info_t* tbuf, const std::vector<io_tensor_desc_t>& io_tensor_desc,
        aipu_tensor_buffer_inner_t& iobuf) const
{
    iobuf.number = io_tensor_desc.size();

    if (nullptr == tbuf)
    {
        return;
    }

    if (iobuf.number != 0)
    {
        iobuf.tensors = new aipu_buffer_t[iobuf.number];
        iobuf.pa = new uint64_t[iobuf.number];
        for (uint32_t i = 0; i < iobuf.number; i++)
        {
            uint32_t sec_iter = io_tensor_desc[i].ref_section_iter;
            iobuf.tensors[i].id = io_tensor_desc[i].id;
            iobuf.tensors[i].size = io_tensor_desc[i].size;
            iobuf.tensors[i].va = (void*)((unsigned long)tbuf->reuse_buf[sec_iter].va +
                io_tensor_desc[i].offset_in_section);
            iobuf.pa[i] = tbuf->reuse_buf[sec_iter].pa + io_tensor_desc[i].offset_in_section;
        }
    }
    else
    {
        iobuf.tensors = nullptr;
        iobuf.pa = nullptr;
    }
}

void AIRT::Graph::destroy_iobuf_info(aipu_tensor_buffer_inner_t& iobuf)
{
    if (iobuf.number != 0)
    {
        iobuf.number = 0;
        if (iobuf.tensors != nullptr)
        {
            delete[] iobuf.tensors;
            iobuf.tensors = nullptr;
        }
        if (iobuf.pa != nullptr)
        {
            delete[] iobuf.pa;
            iobuf.pa = nullptr;
        }
    }
}

void AIRT::Graph::create_tensor_info(const std::vector<io_tensor_desc_t>& desc, aipu_tensor_info_inner_t& info)
{
    info.number = desc.size();
    if (info.number != 0)
    {
        info.desc = new aipu_tensor_desc_t[info.number];
        for (uint32_t i = 0; i < info.number; i++)
        {
            info.desc[i].id = desc[i].id;
            info.desc[i].size = desc[i].size;
            info.desc[i].fmt = desc[i].fmt;
        }
    }
    else
    {
        info.desc = nullptr;
    }
}

void AIRT::Graph::destroy_tensor_info(aipu_tensor_info_inner_t& info)
{
    info.number = 0;
    if (info.desc != nullptr)
    {
        delete[] info.desc;
        info.desc = nullptr;
    }
}

void AIRT::Graph::create_graph_desc(const graph_info_t& info)
{
    gdesc.graph_version = info.version;
    gdesc.building_tool_major = BUILD_MAJOR(info.build_version);
    gdesc.building_tool_minor = BUILD_MINOR(info.build_version);
    gdesc.build_version = BUILD_NUMBER(info.build_version);
    create_tensor_info(info.inputs, gdesc.inputs);
    create_tensor_info(info.outputs, gdesc.outputs);
    create_tensor_info(info.inter_dumps, gdesc.inter_dumps);
}

void AIRT::Graph::destroy_graph_desc()
{
    gdesc.graph_version = 0;
    gdesc.building_tool_major = 0;
    gdesc.building_tool_minor = 0;
    gdesc.build_version = 0;
    destroy_tensor_info(gdesc.inputs);
    destroy_tensor_info(gdesc.outputs);
    destroy_tensor_info(gdesc.inter_dumps);
}

bool AIRT::Graph::is_timeout(struct timeval sched_time, int32_t time_out) const
{
    struct timeval curr;
    long timeuse = 0;
    if (time_out <= 0)
    {
        return false;
    }
    else
    {
        gettimeofday(&curr, NULL);
        timeuse =1000 * (curr.tv_sec - sched_time.tv_sec) +
            (long)(curr.tv_usec/1000 - sched_time.tv_usec/1000);
        return (timeuse > time_out);
    }
}

void AIRT::Graph::dump_job_buffers(const job_desc_t* job, const tbuf_info_t* tbuf, const char* interfix) const
{
    char fname[4096];

    if ((nullptr == job) || (nullptr == tbuf))
    {
        LOG(LOG_WARN, "job or thread buffer not found!");
        return;
    }

    if (job->dump_flag & AIPU_DUMP_TEXT)
    {
        snprintf(fname, sizeof(fname), "%s/Graph0x%x_Job0x%x_%s_Text_Section_Base0x%lx_Size0x%lx_%s.bin",
            job->dump_dir.c_str(), gdesc.id, job->id, interfix,
            pbuf.text.pa - ctrl.get_shm_offset(), pbuf.text.real_size, job->dump_fname_suffix.c_str());
        umd_dump_file_helper(fname, (void*)pbuf.text.va, pbuf.text.real_size);
    }

    if (job->dump_flag & AIPU_DUMP_RO)
    {
        snprintf(fname, sizeof(fname), "%s/Graph0x%x_Job0x%x_%s_Rodata_Section_Base0x%lx_Size0x%lx_%s.bin",
            job->dump_dir.c_str(), gdesc.id, job->id, interfix,
            tbuf->rodata.pa - ctrl.get_shm_offset(), tbuf->rodata.real_size, job->dump_fname_suffix.c_str());
        umd_dump_file_helper(fname, (void*)tbuf->rodata.va, tbuf->rodata.real_size);
    }

    if (job->dump_flag & AIPU_DUMP_STACK)
    {
        snprintf(fname, sizeof(fname), "%s/Graph0x%x_Job0x%x_%s_Stack_Section_Base0x%lx_Size0x%lx_%s.bin",
            job->dump_dir.c_str(), gdesc.id, job->id, interfix,
            tbuf->stack.pa - ctrl.get_shm_offset(), tbuf->stack.real_size, job->dump_fname_suffix.c_str());
        umd_dump_file_helper(fname, (void*)tbuf->stack.va, tbuf->stack.real_size);
    }

    if (job->dump_flag & AIPU_DUMP_STATIC_TENSOR)
    {
        for (uint32_t i = 0; i < pbuf.static_buf.size(); i++)
        {
            snprintf(fname, sizeof(fname), "%s/Graph0x%x_Job0x%x_%s_Static_Section%u_Base0x%lx_Size0x%lx_%s.bin",
                job->dump_dir.c_str(), gdesc.id, job->id, interfix, i,
                pbuf.static_buf[i].pa - ctrl.get_shm_offset(), pbuf.static_buf[i].real_size,
                job->dump_fname_suffix.c_str());
            umd_dump_file_helper(fname, (void*)pbuf.static_buf[i].va, pbuf.static_buf[i].real_size);
        }
    }

    if (job->dump_flag & AIPU_DUMP_REUSE_TENSOR)
    {
        for (uint32_t i = 0; i < tbuf->reuse_buf.size(); i++)
        {
            snprintf(fname, sizeof(fname), "%s/Graph0x%x_Job0x%x_%s_Reuse_Section%u_Base0x%lx_Size0x%lx_%s.bin",
                job->dump_dir.c_str(), gdesc.id, job->id, interfix, i,
                tbuf->reuse_buf[i].pa - ctrl.get_shm_offset(),
                tbuf->reuse_buf[i].real_size, job->dump_fname_suffix.c_str());
            umd_dump_file_helper(fname, (void*)tbuf->reuse_buf[i].va,
                tbuf->reuse_buf[i].real_size);
        }
    }

    /* only dump output/intermediate tensor afret running done */
    if ((job->dump_flag & AIPU_DUMP_OUT_TENSOR) && (interfix[0] == 'A'))
    {
        for (uint32_t i = 0; i < tbuf->iobuf.outputs.number; i++)
        {
            snprintf(fname, sizeof(fname), "%s/Graph0x%x_Job0x%x_%s_OutTensor%u_Base0x%lx_Size0x%x_%s.bin",
                job->dump_dir.c_str(), gdesc.id, job->id, interfix, i,
                tbuf->iobuf.outputs.pa[i] - ctrl.get_shm_offset(),
                tbuf->iobuf.outputs.tensors[i].size, job->dump_fname_suffix.c_str());
            umd_dump_file_helper(fname, (void*)tbuf->iobuf.outputs.tensors[i].va,
                tbuf->iobuf.outputs.tensors[i].size);
        }
    }

    if ((job->dump_flag & AIPU_DUMP_INTER_TENSOR) && (interfix[0] == 'A'))
    {
        for (uint32_t i = 0; i < tbuf->iobuf.inter_dumps.number; i++)
        {
            snprintf(fname, sizeof(fname), "%s/Graph0x%x_Job0x%x_%s_InterTensor%u_Base0x%lx_Size0x%x_%s.bin",
                job->dump_dir.c_str(), gdesc.id, job->id, interfix, i,
                tbuf->iobuf.inter_dumps.pa[i] - ctrl.get_shm_offset(),
                tbuf->iobuf.inter_dumps.tensors[i].size, job->dump_fname_suffix.c_str());
            umd_dump_file_helper(fname, (void*)tbuf->iobuf.inter_dumps.tensors[i].va,
                tbuf->iobuf.inter_dumps.tensors[i].size);
        }
    }
}

void AIRT::Graph::dump_job_mem_map(const job_desc_t* job, const tbuf_info_t* tbuf) const
{
    char bin_fname[4096];
    char txt_fname[4096];
    char buffer[4096];
    mem_dump_buffer_desc_t text_mem;
    mem_dump_buffer_desc_t rodata_mem;
    mem_dump_buffer_desc_t stack_mem;
    mem_dump_data_map_desc_t weight_map;
    std::vector<mem_dump_buffer_desc_t> weight_mem(pbuf.static_buf.size());
    mem_dump_data_map_desc_t reuse_map;
    std::vector<mem_dump_buffer_desc_t> reuse_mem(tbuf->reuse_buf.size());

#if ((defined RTDEBUG) && (RTDEBUG==1))
    std::ofstream bin_of;
#endif
    std::ofstream txt_of;

    uint32_t no_len = 4;
    uint32_t type_len = 8;
    uint32_t id_len = 4;
    uint32_t reg_len = 26 + 16;
    uint32_t size_len = 10;

    uint32_t req_mem = 0;
    uint32_t tot_mem = 0;

    if ((nullptr == job) || (nullptr == tbuf))
    {
        LOG(LOG_WARN, "job or thread buffer not found!");
        return;
    }

    memset(&weight_map, 0, sizeof(mem_dump_data_map_desc_t));
    memset(&reuse_map, 0, sizeof(mem_dump_data_map_desc_t));

    /* fetch buffer info */
    /* text */
    text_mem.dev_pa = pbuf.text.pa - ctrl.get_shm_offset();
    text_mem.req_size = pbuf.text.real_size;
    text_mem.buf_size = pbuf.text.size;
    req_mem += text_mem.req_size;
    tot_mem += text_mem.buf_size;
    /* rodata */
    rodata_mem.dev_pa = tbuf->rodata.pa - ctrl.get_shm_offset();
    rodata_mem.req_size = tbuf->rodata.real_size;
    rodata_mem.buf_size = tbuf->rodata.size;
    req_mem += rodata_mem.req_size;
    tot_mem += rodata_mem.buf_size;
    /* stack */
    stack_mem.dev_pa = tbuf->stack.pa - ctrl.get_shm_offset();
    stack_mem.req_size = tbuf->stack.real_size;
    stack_mem.buf_size = tbuf->stack.size;
    req_mem += stack_mem.req_size;
    tot_mem += stack_mem.buf_size;
    /* static buffers */
    weight_map.number = pbuf.static_buf.size();
    for (uint32_t i = 0; i < pbuf.static_buf.size(); i++)
    {
        weight_mem[i].dev_pa = pbuf.static_buf[i].pa - ctrl.get_shm_offset();
        weight_mem[i].req_size = pbuf.static_buf[i].real_size;
        weight_mem[i].buf_size = pbuf.static_buf[i].size;
        weight_map.tot_req_size += weight_mem[i].req_size;
        weight_map.tot_buf_size += weight_mem[i].buf_size;
    }
    if (CURRENT_AIPU_MALLOC_STRATEGY == AIPU_MALLOC_STRATEGY_GROUP)
    {
        weight_map.tot_buf_size = pbuf.static_group.size;
    }
    req_mem += weight_map.tot_req_size;
    tot_mem += weight_map.tot_buf_size;
    /* reuse buffers */
    reuse_map.number = tbuf->reuse_buf.size();
    for (uint32_t i = 0; i < tbuf->reuse_buf.size(); i++)
    {
        reuse_mem[i].dev_pa = tbuf->reuse_buf[i].pa - ctrl.get_shm_offset();
        reuse_mem[i].req_size = tbuf->reuse_buf[i].real_size;
        reuse_mem[i].buf_size = tbuf->reuse_buf[i].size;
        reuse_map.tot_req_size += reuse_mem[i].req_size;
        reuse_map.tot_buf_size += reuse_mem[i].buf_size;
    }
    if (CURRENT_AIPU_MALLOC_STRATEGY == AIPU_MALLOC_STRATEGY_GROUP)
    {
        reuse_map.tot_buf_size = tbuf->reuse_group.size;
    }
    req_mem += reuse_map.tot_req_size;
    tot_mem += reuse_map.tot_buf_size;

    /* create files & write info */
    snprintf(bin_fname, sizeof(bin_fname), "%s/Graph0x%x_Job0x%x_Mem_Map_%s.bin",
        job->dump_dir.c_str(), gdesc.id, job->id, job->dump_fname_suffix.c_str());
    snprintf(txt_fname, sizeof(txt_fname), "%s/Graph0x%x_Job0x%x_Mem_Map_%s.txt",
        job->dump_dir.c_str(), gdesc.id, job->id, job->dump_fname_suffix.c_str());
#if ((defined RTDEBUG) && (RTDEBUG==1))
    bin_of.open(bin_fname, std::ios::out | std::ios::binary);
    if (!bin_of.is_open())
    {
        LOG(LOG_ERR, "create binary mem_map dump file failed!");
        return;
    }
#endif
    txt_of.open(txt_fname, std::ios::out);
    if(!txt_of.is_open())
    {
        LOG(LOG_ERR, "create text mem_map dump file failed!");
        return;
    }

    snprintf(buffer, sizeof(buffer), "\t\t\tAIPU UMD Memory Map Dump\n");
    txt_of.write(buffer, strlen(buffer));
    umd_draw_line_helper(txt_of, '=', 80);
    snprintf(buffer, sizeof(buffer), "Graph ID: 0x%x\n", gdesc.id);
    txt_of.write(buffer, strlen(buffer));
    snprintf(buffer, sizeof(buffer), "Job ID:   0x%x\n", job->id);
    txt_of.write(buffer, strlen(buffer));
    snprintf(buffer, sizeof(buffer), "\n%-*s%-*s\n", 8, "Memory", 30, "Requested/Allocated (Bytes)");
    txt_of.write(buffer, strlen(buffer));
    umd_draw_line_helper(txt_of, '-', 40);
    snprintf(buffer, sizeof(buffer), "%-*s0x%x/0x%x\n", 8, "Total", req_mem, tot_mem);
    txt_of.write(buffer, strlen(buffer));
    snprintf(buffer, sizeof(buffer), "%-*s0x%x/0x%x\n", 8, "Text",
            text_mem.req_size, text_mem.buf_size);
    txt_of.write(buffer, strlen(buffer));
    snprintf(buffer, sizeof(buffer), "%-*s0x%x/0x%x\n", 8, "Rodata",
            rodata_mem.req_size, rodata_mem.buf_size);
    txt_of.write(buffer, strlen(buffer));
    snprintf(buffer, sizeof(buffer), "%-*s0x%x/0x%x\n", 8, "Stack",
            stack_mem.req_size, stack_mem.buf_size);
    txt_of.write(buffer, strlen(buffer));
    snprintf(buffer, sizeof(buffer), "%-*s0x%x/0x%x\n", 8, "Static",
            weight_map.tot_req_size, weight_map.tot_buf_size);
    txt_of.write(buffer, strlen(buffer));
    snprintf(buffer, sizeof(buffer), "%-*s0x%x/0x%x\n", 8, "Reuse",
            reuse_map.tot_req_size, reuse_map.tot_buf_size);
    txt_of.write(buffer, strlen(buffer));
    umd_draw_line_helper(txt_of, '-', 40);
    snprintf(buffer, sizeof(buffer), "\n\t\tDetailed Buffer Allocation Information\n");
    txt_of.write(buffer, strlen(buffer));
    umd_draw_line_helper(txt_of, '-', 80);
    snprintf(buffer, sizeof(buffer), "%-*s%-*s%-*s%-*s%-*s%-*s\n",
        no_len, "No.", type_len, "Type", id_len, "ID", reg_len, "Valid Address Space",
        12, "Requested", 10, "Allocated");
    txt_of.write(buffer, strlen(buffer));
    umd_draw_line_helper(txt_of, '-', 80);

    /* text */
#if ((defined RTDEBUG) && (RTDEBUG==1))
    bin_of.write((char*)&text_mem, sizeof(mem_dump_buffer_desc_t));
#endif
    snprintf(buffer, sizeof(buffer), "%-*d%-*s%-*d[0x%016lx, 0x%016lx]  0x%-*x0x%-*x\n",
        no_len, 0, type_len, "text", id_len, 0, text_mem.dev_pa, text_mem.dev_pa + text_mem.req_size - 1,
        size_len, text_mem.req_size, size_len, text_mem.buf_size);
    txt_of.write(buffer, strlen(buffer));
    /* rodata */
#if ((defined RTDEBUG) && (RTDEBUG==1))
    bin_of.write((char*)&rodata_mem, sizeof(mem_dump_buffer_desc_t));
#endif
    snprintf(buffer, sizeof(buffer), "%-*d%-*s%-*d[0x%016lx, 0x%016lx]  0x%-*x0x%-*x\n",
        no_len, 1, type_len, "rodata", id_len, 0, rodata_mem.dev_pa, rodata_mem.dev_pa + rodata_mem.req_size - 1,
        size_len, rodata_mem.req_size, size_len, rodata_mem.buf_size);
    txt_of.write(buffer, strlen(buffer));
    /* stack */
#if ((defined RTDEBUG) && (RTDEBUG==1))
    bin_of.write((char*)&stack_mem, sizeof(mem_dump_buffer_desc_t));
#endif
    snprintf(buffer, sizeof(buffer), "%-*d%-*s%-*d[0x%016lx, 0x%016lx]  0x%-*x0x%-*x\n",
        no_len, 2, type_len, "stack", id_len, 0, stack_mem.dev_pa, stack_mem.dev_pa + stack_mem.req_size - 1,
        size_len, stack_mem.req_size, size_len, stack_mem.buf_size);
    txt_of.write(buffer, strlen(buffer));
    /* static buffers */
#if ((defined RTDEBUG) && (RTDEBUG==1))
    bin_of.write((char*)&weight_map, sizeof(mem_dump_data_map_desc_t));
#endif
    for (uint32_t i = 0; i < weight_mem.size(); i++)
    {
#if ((defined RTDEBUG) && (RTDEBUG==1))
        bin_of.write((char*)&weight_mem[i], sizeof(mem_dump_buffer_desc_t));
#endif
        snprintf(buffer, sizeof(buffer), "%-*d%-*s%-*d[0x%016lx, 0x%016lx]  0x%-*x0x%-*x\n",
            no_len, 3+i, type_len, "static", id_len, i, weight_mem[i].dev_pa,
            weight_mem[i].dev_pa + weight_mem[i].req_size - 1,
            size_len, weight_mem[i].req_size, size_len, weight_mem[i].buf_size);
        txt_of.write(buffer, strlen(buffer));
    }
    /* reuse buffers */
#if ((defined RTDEBUG) && (RTDEBUG==1))
    bin_of.write((char*)&reuse_map, sizeof(mem_dump_data_map_desc_t));
#endif
    for (uint32_t i = 0; i < reuse_mem.size(); i++)
    {
#if ((defined RTDEBUG) && (RTDEBUG==1))
        bin_of.write((char*)&reuse_mem[i], sizeof(mem_dump_buffer_desc_t));
#endif
        snprintf(buffer, sizeof(buffer), "%-*d%-*s%-*d[0x%016lx, 0x%016lx]  0x%-*x0x%-*x\n",
            no_len, 3+(int)weight_mem.size()+i, type_len, "reuse", id_len, i, reuse_mem[i].dev_pa,
            reuse_mem[i].dev_pa + reuse_mem[i].req_size - 1,
            size_len, reuse_mem[i].req_size, size_len, reuse_mem[i].buf_size);
        txt_of.write(buffer, strlen(buffer));
    }
    umd_draw_line_helper(txt_of, '-', 80);
    umd_draw_line_helper(txt_of, '=', 80);

#if ((defined RTDEBUG) && (RTDEBUG==1))
    bin_of.close();
#endif
    txt_of.close();
}

uint32_t AIRT::Graph::create_buf_handle_inner() const
{
    uint32_t id_candidate = gdesc.id << 16;
    while (tbufs.count(id_candidate) == 1)
    {
        id_candidate++;
    }
    return id_candidate;
}

uint32_t AIRT::Graph::create_job_id_inner() const
{
    uint32_t id_candidate = gdesc.id << 16;
    while (is_job_exist_inner(id_candidate))
    {
        id_candidate++;
    }
    return id_candidate;
}

bool AIRT::Graph::is_unload_ok()
{
    bool ret = false;
    pthread_rwlock_rdlock(&job_queue_lock);
    ret = is_all_jobs_end_inner();
    pthread_rwlock_unlock(&job_queue_lock);
    return ret;
}

void AIRT::Graph::get_graph_desc(aipu_graph_desc_t* gdesc_user) const
{
    if (nullptr != gdesc_user)
    {
        memcpy(gdesc_user, &gdesc, sizeof(aipu_graph_desc_t));
    }
}

aipu_status_t AIRT::Graph::is_job_sched(uint32_t job_id)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    job_desc_t* job = get_job_ptr(job_id);

    if (nullptr == job)
    {
        return AIPU_STATUS_ERROR_JOB_NOT_EXIST;
    }

    if (job->state == JOB_STATE_BUILT)
    {
        return AIPU_STATUS_ERROR_JOB_NOT_SCHED;
    }

    return ret;
}

bool AIRT::Graph::is_job_end(uint32_t job_id)
{
    job_desc_t* job = get_job_ptr(job_id);

    if (nullptr == job)
    {
        return false;
    }

    if ((job->state == JOB_STATE_DONE) ||
        (job->state == JOB_STATE_EXCEPTION) ||
        (job->state == JOB_STATE_TIMEOUT))
    {
        return true;
    }

    return false;
}

aipu_status_t AIRT::Graph::load(const graph_info_t& info, bool _map_flag)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
#if (defined X86_LINUX) && (X86_LINUX==1)
    tbuf_info_t* tbuf = nullptr;
    uint32_t handle = create_buf_handle_inner();
#else
    buffer_desc_t buf;
#endif
    /**
     * No lock in load because load operation will be done before any
     * API reference to this graph
     */

    map_flag = _map_flag;
    if (map_flag)
    {
        gbin = info.gbin;
        gbin_size = info.gbin_size;
    }
    arch = AIPU_ARCH(info.device);
    hw_version = AIPU_VERSION(info.device);
    hw_config = AIPU_CONFIG(info.device);
    entry = info.entry;
    asid_flag = info.asid_flag;
    linked_list_flag = info.linked_list_flag;
    tbuf_templ = info.tbuf_templ;
    inputs = info.inputs;
    outputs = info.outputs;
    inter_dumps = info.inter_dumps;
    pdata = info.pdata;
    plog_data = info.plog_data;
    param_map = info.param_map;
    dcr_map = info.dcr_map;
    create_graph_desc(info);

    /* alloc and load text buffer */
    ret = ctrl.alloc_text_buffer(gdesc.id, info.pbuf_templ, pbuf.text);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }
    ret = ctrl.load_text_buffer(gdesc.id, info.pbuf_templ.text_src, info.pbuf_templ.text_size, pbuf.text);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }

#if (defined X86_LINUX) && (X86_LINUX==1)
    tbuf = new tbuf_info_t;
    ret = ctrl.simulation_alloc_data_buffer(gdesc.id, info.pbuf_templ, pbuf, info.tbuf_templ, *tbuf);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }
    /* calculate io buffer address */
    create_iobuf_info(tbuf, inputs, tbuf->iobuf.inputs);
    create_iobuf_info(tbuf, outputs, tbuf->iobuf.outputs);
    create_iobuf_info(tbuf, inter_dumps, tbuf->iobuf.inter_dumps);
    create_iobuf_info(tbuf, pdata, tbuf->iobuf.pdata);
    create_iobuf_info(tbuf, plog_data, tbuf->iobuf.plog_data);

    ret = ctrl.simulation_set_io_info(gdesc.id, tbuf->iobuf);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        delete tbuf;
        goto finish;
    }
    tbuf->handle = handle;
    tbuf->is_free = true;
    tbufs[handle] = tbuf;
#else
    if (CURRENT_AIPU_MALLOC_STRATEGY == AIPU_MALLOC_STRATEGY_GROUP)
    {
        /* a simple buffer offset computation method meets all size & alignment requirements */
        ret = alloc_group_buffers(info.pbuf_templ.static_sections, AIPU_MM_DATA_TYPE_STATIC,
            pbuf.static_buf, pbuf.static_group);
        if (AIPU_STATUS_SUCCESS != ret)
        {
            goto finish;
        }
    }
    else if (CURRENT_AIPU_MALLOC_STRATEGY == AIPU_MALLOC_STRATEGY_SEPARATED)
    {
        for (uint32_t i = 0; i < info.pbuf_templ.static_sections.size(); i++)
        {
            ret = ctrl.malloc_buf(AIPU_MM_DATA_TYPE_STATIC, info.pbuf_templ.static_sections[i].size,
                info.pbuf_templ.static_sections[i].align_in_page, &buf, 0);
            if (AIPU_STATUS_SUCCESS != ret)
            {
                goto finish;
            }
            pbuf.static_buf.push_back(buf);
        }
        pbuf.static_group.va = nullptr;
        pbuf.static_group.pa = 0;
        pbuf.static_group.size = 0;
        pbuf.static_group.real_size = 0;
    }
#endif

    for (uint32_t i = 0; i < info.pbuf_templ.static_sections.size(); i++)
    {
        ctrl.load_buffer(pbuf.static_buf[i].va, info.pbuf_templ.static_sections[i].load_src,
            info.pbuf_templ.static_sections[i].size);
    }

finish:
    return ret;
}

aipu_status_t AIRT::Graph::alloc_group_buffers(const std::vector<section_desc_t>& sections,
        uint32_t dtype, std::vector<buffer_desc_t>& buffers, buffer_desc_t& group)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    buffer_desc_t group_buf, child_buf;
    uint32_t cnt = sections.size();
    uint32_t* offsets = nullptr;
    uint32_t tot_bytes = 0;

    if (cnt)
    {
        offsets = new uint32_t[cnt];
        offsets[0] = 0;
        for (uint32_t i = 1; i < cnt; i++)
        {
            offsets[i] = get_aligned_addr(offsets[i - 1] + sections[i - 1].size,
               sections[i].align_in_page);
        }
        tot_bytes = offsets[cnt - 1] + sections[cnt - 1].size;

        ret = ctrl.malloc_buf(dtype, tot_bytes, sections[0].align_in_page,
                &group_buf);
        if (AIPU_STATUS_SUCCESS != ret)
        {
            goto finish;
        }

        group = group_buf;
        LOG(LOG_CLOSE, "group buffer pa = 0x%lx, size 0x%lx", group_buf.pa,
                group_buf.size);

        for (uint32_t i = 0; i < cnt; i++)
        {
            child_buf.pa = group_buf.pa + offsets[i];
            child_buf.va = (void*)((unsigned long)group_buf.va + offsets[i]);
            child_buf.size = sections[i].size;
            child_buf.real_size = sections[i].size;
            buffers.push_back(child_buf);
        }
    }

finish:
    return ret;
}

aipu_status_t AIRT::Graph::unload()
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    std::map<uint32_t, tbuf_info_t*>::iterator tbuf_iter;
    std::map<uint32_t, job_desc_t*>::iterator job_iter;

    /**
     * No lock in unload because unload operation will only be done
     * when no API reference this graph
     */

    if (!is_unload_ok())
    {
        ret = AIPU_STATUS_ERROR_INVALID_OP;
        goto finish;
    }

    for(job_iter = jobs.begin(); job_iter != jobs.end(); job_iter++)
    {
        delete job_iter->second;
        job_iter->second = nullptr;
    }
    jobs.clear();

    param_map.clear();
    inputs.clear();
    outputs.clear();
    inter_dumps.clear();
    plog_data.clear();

    if (nullptr != pbuf.text.va)
    {
        ret = ctrl.free_text_buffer(pbuf.text);
        if (AIPU_STATUS_SUCCESS != ret)
        {
            goto finish;
        }
        pbuf.text.pa = 0;
        pbuf.text.va = nullptr;
        pbuf.text.size = 0;
    }

#if (defined ARM_LINUX) && (ARM_LINUX==1)
    if (CURRENT_AIPU_MALLOC_STRATEGY == AIPU_MALLOC_STRATEGY_GROUP)
    {
        if (nullptr != pbuf.static_group.va)
        {
            ret = ctrl.free_buf(&pbuf.static_group);
            if (AIPU_STATUS_SUCCESS != ret)
            {
                goto finish;
            }
        }
    }
    else if (CURRENT_AIPU_MALLOC_STRATEGY == AIPU_MALLOC_STRATEGY_SEPARATED)
    {
        for (uint32_t i = 0; i < pbuf.static_buf.size(); i++)
        {
            ret = ctrl.free_buf(&pbuf.static_buf[i]);
            if (AIPU_STATUS_SUCCESS != ret)
            {
                goto finish;
            }
        }
    }
    pbuf.static_buf.clear();
#endif

    for (tbuf_iter = tbufs.begin(); tbuf_iter != tbufs.end(); tbuf_iter++)
    {
        ret = free_thread_buffer(tbuf_iter->first);
        if (AIPU_STATUS_SUCCESS != ret)
        {
            goto finish;
        }
    }

    if (map_flag && (nullptr != gbin))
    {
        munmap(gbin, gbin_size);
        gbin = nullptr;
        gbin_size = 0;
    }

    ctrl.unload_graph(gdesc.id);
    destroy_graph_desc();

finish:
    return ret;
}

aipu_status_t AIRT::Graph::alloc_thread_buffer(aipu_buffer_alloc_info_t* info)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    uint32_t handle = 0;
    tbuf_info_t* tbuf = nullptr;
#if (defined X86_LINUX) && (X86_LINUX==1)
    std::map<uint32_t, tbuf_info_t*>::const_iterator tbuf_iter;
#else
    buffer_desc_t buf;
#endif

    if (nullptr == info)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

#if (defined X86_LINUX) && (X86_LINUX==1)
    /* has been allocated; just get handle here */
    /* for x86 simulation, at most one tbuf exists for one graph object */
    for (tbuf_iter = tbufs.begin(); tbuf_iter != tbufs.end(); tbuf_iter++)
    {
        handle = tbuf_iter->first;
        tbuf = tbuf_iter->second;
    }
    if (nullptr == tbuf)
    {
        ret = AIPU_STATUS_ERROR_BUF_ALLOC_FAIL;
        goto finish;
    }
#else
    tbuf = new tbuf_info_t;
    /* initialize tbuf */
    /* stack */
    ret = ctrl.malloc_buf(AIPU_MM_DATA_TYPE_RO_STACK, tbuf_templ.stack_size, tbuf_templ.stack_align_in_page,
        &tbuf->stack, pbuf.text.region_id);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto delete_tbuf;
    }
    /* rodata & descriptor */
    ret = ctrl.alloc_rodata_buffer(pbuf.text.region_id, tbuf_templ, tbuf->rodata, tbuf->descriptor);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto free_stack;
    }

    /* reuse buffers */
    if (CURRENT_AIPU_MALLOC_STRATEGY == AIPU_MALLOC_STRATEGY_GROUP)
    {
        /* a simple buffer offset computation method meets all size & alignment requirements */
        ret = alloc_group_buffers(tbuf_templ.reuse_sections, AIPU_MM_DATA_TYPE_REUSE,
            tbuf->reuse_buf, tbuf->reuse_group);
        if (AIPU_STATUS_SUCCESS != ret)
        {
            goto free_ro_reuse;
        }
    }
    else if (CURRENT_AIPU_MALLOC_STRATEGY == AIPU_MALLOC_STRATEGY_SEPARATED)
    {
        for (uint32_t i = 0; i < tbuf_templ.reuse_sections.size(); i++)
        {
            ret = ctrl.malloc_buf(AIPU_MM_DATA_TYPE_REUSE, tbuf_templ.reuse_sections[i].size,
                tbuf_templ.reuse_sections[i].align_in_page, &buf, 0);
            if (AIPU_STATUS_SUCCESS != ret)
            {
                goto free_ro_reuse;
            }
            tbuf->reuse_buf.push_back(buf);
        }
    }
    /* other members */
    tbuf->is_free = true;
    create_iobuf_info(tbuf, inputs, tbuf->iobuf.inputs);
    create_iobuf_info(tbuf, outputs, tbuf->iobuf.outputs);
    create_iobuf_info(tbuf, inter_dumps, tbuf->iobuf.inter_dumps);
    create_iobuf_info(tbuf, pdata, tbuf->iobuf.pdata);
    create_iobuf_info(tbuf, plog_data, tbuf->iobuf.plog_data);

    /* success in allocation */
    pthread_rwlock_wrlock(&tbuf_lock);
    handle = create_buf_handle_inner();
    tbuf->handle = handle;
    tbufs[handle] = tbuf;
    pthread_rwlock_unlock(&tbuf_lock);
#endif

    /* initialize first 8 char in printf buffer */
    for (uint32_t i = 0; i < tbuf->iobuf.plog_data.number; i++)
    {
        uint32_t header_len = 8;
        void* dest = tbuf->iobuf.plog_data.tensors[i].va;
        for (uint32_t i = 0; i < header_len; i++)
        {
            *(volatile char*)((unsigned long)dest + i) = 0;
        }
    }

    /* success */
    info->handle = handle;
    info->inputs.number   = tbuf->iobuf.inputs.number;
    info->inputs.tensors  = tbuf->iobuf.inputs.tensors;
    info->outputs.number  = tbuf->iobuf.outputs.number;
    info->outputs.tensors = tbuf->iobuf.outputs.tensors;
    info->inter_dumps.number    = tbuf->iobuf.inter_dumps.number;
    info->inter_dumps.tensors   = tbuf->iobuf.inter_dumps.tensors;
    info->printf_dumps.number    = tbuf->iobuf.plog_data.number;
    info->printf_dumps.tensors   = tbuf->iobuf.plog_data.tensors;
    info->profiler_dumps.number  = tbuf->iobuf.pdata.number;
    info->profiler_dumps.tensors = tbuf->iobuf.pdata.tensors;
    goto finish;

#if (defined ARM_LINUX) && (ARM_LINUX==1)
free_ro_reuse:
    ctrl.free_buf(&tbuf->rodata);
    for (uint32_t i = 0; i < tbuf->reuse_buf.size(); i++)
    {
        ctrl.free_buf(&tbuf->reuse_buf[i]);
    }

free_stack:
    ctrl.free_buf(&tbuf->stack);

delete_tbuf:
    delete tbuf;
    tbuf = nullptr;
#endif
finish:
    return ret;
}

aipu_status_t AIRT::Graph::free_thread_buffer(uint32_t handle)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    tbuf_info_t* tbuf = get_tbuf_ptr(handle);

    if (nullptr == tbuf)
    {
        ret = AIPU_STATUS_ERROR_INVALID_HANDLE;
        goto finish;
    }

    if (!tbuf->is_free)
    {
        ret = AIPU_STATUS_ERROR_BUSY_HANDLE;
        goto finish;
    }

    /**
     * this free func is called in both unload & free tensor buffer APIs
     * therefore both device shared buffer & heap buffers should be freed here
     */

    /* free cpu & aipu shared device buffers */
    /* rodata */
    ret = ctrl.free_buf(&tbuf->rodata);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }

#if (defined ARM_LINUX) && (ARM_LINUX==1)
    /* stack */
    ret = ctrl.free_buf(&tbuf->stack);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }

    /* reuse buffers */
    if (CURRENT_AIPU_MALLOC_STRATEGY == AIPU_MALLOC_STRATEGY_GROUP)
    {
        ret = ctrl.free_buf(&tbuf->reuse_group);
        if (AIPU_STATUS_SUCCESS != ret)
        {
            goto finish;
        }
    }
    else if (CURRENT_AIPU_MALLOC_STRATEGY == AIPU_MALLOC_STRATEGY_SEPARATED)
    {
        for (uint32_t i = 0; i < tbuf->reuse_buf.size(); i++)
        {
            ret = ctrl.free_buf(&tbuf->reuse_buf[i]);
            if (AIPU_STATUS_SUCCESS != ret)
            {
                goto finish;
            }
        }
    }
#else
    ret = ctrl.simulation_free_data_buffer(gdesc.id);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }
#endif

    /* free cpu heap buffers */
    pthread_rwlock_wrlock(&tbuf_lock);
    destroy_iobuf_info(tbuf->iobuf.inputs);
    destroy_iobuf_info(tbuf->iobuf.outputs);
    destroy_iobuf_info(tbuf->iobuf.inter_dumps);
    destroy_iobuf_info(tbuf->iobuf.plog_data);
    delete tbuf;
    tbuf = nullptr;
    tbufs.erase(handle);
    pthread_rwlock_unlock(&tbuf_lock);

finish:
    return ret;
}

bool AIRT::Graph::is_asid_enabled() const
{
    return (hw_version == AIPU_HW_VERSION_ZHOUYI_V2) && IS_ASID_ENABLED(asid_flag);
}

aipu_status_t AIRT::Graph::build_new_job(uint32_t handle, uint32_t* job_id)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    job_desc_t* job = nullptr;
    tbuf_info_t* tbuf = get_tbuf_ptr(handle);

    if (nullptr == job_id)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    if (nullptr == tbuf)
    {
        ret = AIPU_STATUS_ERROR_INVALID_HANDLE;
        goto finish;
    }

    if (!tbuf->is_free)
    {
        ret = AIPU_STATUS_ERROR_BUSY_HANDLE;
        goto finish;
    }

    tbuf->is_free = false;

    /* load initial values into rodata & descriptor buffer */
    ctrl.load_buffer(tbuf->rodata.va, tbuf_templ.rodata_src, tbuf_templ.rodata_size);
    if (linked_list_flag != 0)
    {
        ctrl.load_buffer(tbuf->descriptor.va, tbuf_templ.dcr_src, tbuf_templ.dcr_size);
    }

    /* update address info in rodata & descriptor buffer */
    /* rodata */
    for (uint32_t i = 0; i < param_map.size(); i++)
    {
        volatile uint32_t* dest = (uint32_t*)((unsigned long)tbuf->rodata.va +
            param_map[i].offset_in_map);
        uint32_t ref_iter = param_map[i].ref_section_iter;
        uint32_t sec_offset = param_map[i].sub_section_offset;
        uint32_t sub_sec_pa = 0;
        if (param_map[i].offset_in_map >= tbuf->rodata.size)
        {
            ret = AIPU_STATUS_ERROR_INVALID_SIZE;
            goto finish;
        }

        if (param_map[i].load_type == PARAM_MAP_LOAD_TYPE_REUSE)
        {
            if (ref_iter >= tbuf->reuse_buf.size())
            {
                ret = AIPU_STATUS_ERROR_INVALID_SIZE;
                goto finish;
            }
            sub_sec_pa = host2dev(tbuf->reuse_buf[ref_iter].pa + sec_offset);
        }
        else if (param_map[i].load_type == PARAM_MAP_LOAD_TYPE_STATIC)
        {
            if (ref_iter >= pbuf.static_buf.size())
            {
                ret = AIPU_STATUS_ERROR_INVALID_SIZE;
                goto finish;
            }
            sub_sec_pa = host2dev(pbuf.static_buf[ref_iter].pa + sec_offset);
        }

        if (is_asid_enabled())
        {
            if (param_map[i].load_type == PARAM_MAP_LOAD_TYPE_REUSE)
            {
                sub_sec_pa -= tbuf->reuse_group.pa;
            }
            else if (param_map[i].load_type == PARAM_MAP_LOAD_TYPE_STATIC)
            {
                sub_sec_pa -= pbuf.static_group.pa;
            }
        }

        *dest = ((sub_sec_pa & param_map[i].addr_mask) | (*dest & (~param_map[i].addr_mask)));
    }
    /* descriptor */
    if (linked_list_flag != 0)
    {
        for (uint32_t i = 0; i < dcr_map.size(); i++)
        {
            void* dest = (void*)((unsigned long)get_base_va(dcr_map[i].type, tbuf) +
                dcr_map[i].next_addr_entry_offset);
            uint32_t pa = host2dev(get_base_pa(dcr_map[i].next_type, tbuf) + dcr_map[i].next_offset);
            *(uint32_t*)dest = pa;
        }
    }

    job = new job_desc_t;
    memset(job, 0, sizeof(job_desc_t));
    job->state = JOB_STATE_BUILT;
    job->buf_handle = handle;
    job->dump_flag = 0;
    job->config.arch = arch;
    job->config.hw_version = hw_version;
    job->config.hw_config = hw_config;
    job->config.enable_asid = is_asid_enabled();
    job->config.code.instruction_base_pa = host2dev(pbuf.text.pa);
    job->config.code.start_pc_pa = job->config.code.instruction_base_pa + entry;
    job->config.code.interrupt_pc_pa = job->config.code.instruction_base_pa + 0x10;
    job->config.code_size = pbuf.text.size;
    job->config.rodata_base = host2dev(tbuf->rodata.pa);
    job->config.rodata_size = tbuf->rodata.size;
    job->config.stack_base = host2dev(tbuf->stack.pa);
    job->config.stack_size = tbuf->stack.size;
    job->config.static_base = host2dev(pbuf.static_group.pa);
    job->config.static_size = pbuf.static_group.size;
    job->config.reuse_base = host2dev(tbuf->reuse_group.pa);
    job->config.reuse_size = tbuf->reuse_group.size;
    job->lock = PTHREAD_MUTEX_INITIALIZER;
    job->cond = PTHREAD_COND_INITIALIZER;

    pthread_rwlock_wrlock(&job_queue_lock);
    job->id = create_job_id_inner();
    jobs[job->id] = job;
    pthread_rwlock_unlock(&job_queue_lock);

    /* success */
    *job_id = job->id;

finish:
    return ret;
}

aipu_status_t AIRT::Graph::flush_job(uint32_t job_id)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    job_desc_t* job = get_job_ptr(job_id);

    if (nullptr == job)
    {
        return AIPU_STATUS_ERROR_JOB_NOT_EXIST;
    }

    if (job->state != JOB_STATE_BUILT)
    {
        ret = AIPU_STATUS_ERROR_JOB_SCHED;
        goto finish;
    }

    if (job->dump_flag & AIPU_DUMP_BEFORE_RUN)
    {
        dump_job_buffers(job, get_tbuf_ptr(job->buf_handle), "Before_Run");
    }

    if (job->dump_flag & AIPU_DUMP_MEM_MAP)
    {
        dump_job_mem_map(job, get_tbuf_ptr(job->buf_handle));
    }

    if (job->dump_flag & AIPU_DUMP_DRV_PROF_DATA)
    {
        job->config.enable_prof = 1;
    }

    /* update state before scheduling for safety and compatibility considerations */
    pthread_rwlock_wrlock(&job_queue_lock);
    job->state = JOB_STATE_SCHED;
    sched.push_back(job->id);
    pthread_rwlock_unlock(&job_queue_lock);

    ret = ctrl.schedule_job_on_aipu(gdesc.id, job);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        pthread_rwlock_wrlock(&job_queue_lock);
        delete_from_sched_queue_inner(job_id);
        pthread_rwlock_unlock(&job_queue_lock);
        goto finish;
    }

    /* success */
    gettimeofday(&job->timeout_start, NULL);

finish:
    return ret;
}

void AIRT::Graph::set_timespec(struct timespec* time, struct timeval* curr, uint32_t time_out) const
{
    long nsec = 0;
    if ((time_out != 0) && (time != nullptr) && (curr != nullptr))
    {
        nsec = curr->tv_usec * 1000 + (time_out % 1000) * 1000000;
        time->tv_sec = curr->tv_sec + nsec / 1000000000 + time_out / 1000;
        time->tv_nsec = nsec % 1000000000;
    }
}

aipu_status_t AIRT::Graph::wait_for_job_end_sleep(uint32_t job_id, int32_t time_out,
    aipu_job_status_t* status)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    struct timespec time;
    struct timeval curr;
    job_desc_t* job = get_job_ptr(job_id);

    if (nullptr == status)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    if (nullptr == job)
    {
        *status = (aipu_job_status_t)JOB_STATE_NO_STATE;
        ret = AIPU_STATUS_ERROR_JOB_NOT_EXIST;
        goto finish;
    }

    if ((job->state == JOB_STATE_BUILT) ||
        (job->state == JOB_STATE_TIMEOUT))
    {
        *status = (aipu_job_status_t)JOB_STATE_NO_STATE;
        ret = AIPU_STATUS_ERROR_JOB_NOT_SCHED;
        goto finish;
    }

    pthread_mutex_lock(&job->lock);
    if (time_out <= 0)
    {
        while ((job->state != JOB_STATE_DONE) &&
                (job->state != JOB_STATE_EXCEPTION))
        {
            pthread_cond_wait(&job->cond, &job->lock);
        }
    }
    else
    {
        gettimeofday(&curr, NULL);
        set_timespec(&time, &curr, time_out);
        LOG(LOG_INFO, "sleep: %lds, %ldns", time.tv_sec, time.tv_nsec);
        pthread_cond_timedwait(&job->cond, &job->lock, &time);
        LOG(LOG_INFO, "wake up");
    }
    pthread_mutex_unlock(&job->lock);

    if (job->state == JOB_STATE_DONE)
    {
        ret = AIPU_STATUS_SUCCESS;
    }
    else if(job->state == JOB_STATE_EXCEPTION)
    {
        ret = AIPU_STATUS_ERROR_JOB_EXCEPTION;
    }
    else
    {
        ctrl.kill_timeout_job(job_id);
        job->state = JOB_STATE_TIMEOUT;
        ret = AIPU_STATUS_ERROR_JOB_TIMEOUT;
        LOG(LOG_INFO, "return timeout");
    }
    *status = (aipu_job_status_t)job->state;

finish:
    return ret;
}

void AIRT::Graph::dump_end_job_buffers(uint32_t job_id)
{
    char dump_dir[1024];
#if (defined ARM_LINUX) && (ARM_LINUX==1)
    char fname[4096];
    char buffer[4096];
    std::ofstream of;
#endif

    job_desc_t* job = get_job_ptr(job_id);
    tbuf_info_t* tbuf = NULL;
    if (nullptr == job)
    {
        return;
    }
    tbuf = get_tbuf_ptr(job->buf_handle);
    if (nullptr == tbuf)
    {
        return;
    }

    if (job->dump_flag & AIPU_DUMP_AFTER_RUN)
    {
        dump_job_buffers(job, tbuf, "After_Run");
    }

    if (job->dump_flag == 0)
    {
        strcpy(dump_dir, ".");
    }
    else
    {
        strcpy(dump_dir, job->dump_dir.c_str());
    }

#if (defined ARM_LINUX) && (ARM_LINUX==1)
    /* dump profiling data collected by KMD */
    if (job->dump_flag & AIPU_DUMP_DRV_PROF_DATA)
    {
        snprintf(fname, sizeof(fname), "%s/Graph0x%x_Job0x%x_DriverProfilingData_%s.txt",
            dump_dir, gdesc.id, job->id, job->dump_fname_suffix.c_str());
        of.open(fname, std::ios::out | std::ios::binary);
        if ((!of.is_open()) || (!of.is_open()))
        {
            LOG(LOG_ERR, "create profiling data dump file failed!");
            return;
        }
        snprintf(buffer, sizeof(buffer), "\tAIPU KMD Profiling Data Dump\n");
        of.write(buffer, strlen(buffer));
        umd_draw_line_helper(of, '=', 40);
        snprintf(buffer, sizeof(buffer), "Graph ID: 0x%x\n", gdesc.id);
        of.write(buffer, strlen(buffer));
        snprintf(buffer, sizeof(buffer), "Job ID:   0x%x\n", job->id);
        of.write(buffer, strlen(buffer));

        snprintf(buffer, sizeof(buffer), "Time Consuming: %.3fms\n",
            ((double)job->pdata.execution_time_ns/1000000));
        of.write(buffer, strlen(buffer));
        snprintf(buffer, sizeof(buffer), "\n\tProfiling Register Value(s)\n");
        of.write(buffer, strlen(buffer));
        umd_draw_line_helper(of, '-', 40);
        snprintf(buffer, sizeof(buffer), "TOT RDATA: 0x%016lx\n",
            ((uint64_t)job->pdata.rdata_tot_msb<<32) + job->pdata.rdata_tot_lsb);
        of.write(buffer, strlen(buffer));
        snprintf(buffer, sizeof(buffer), "TOT WDATA: 0x%016lx\n",
                ((uint64_t)job->pdata.wdata_tot_msb << 32) + job->pdata.wdata_tot_lsb);
        of.write(buffer, strlen(buffer));
        snprintf(buffer, sizeof(buffer), "TOT CYCLE: 0x%016lx\n",
                ((uint64_t)job->pdata.tot_cycle_msb << 32) + job->pdata.tot_cycle_lsb);
        of.write(buffer, strlen(buffer));
        umd_draw_line_helper(of, '-', 40);
        umd_draw_line_helper(of, '=', 40);
        of.close();
    }
#endif
}

aipu_status_t AIRT::Graph::update_job_status(job_status_desc* status, bool is_wake_up)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    job_desc_t* job = nullptr;

    if (nullptr == status)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    job = get_job_ptr(status->job_id);
    if (nullptr == job)
    {
        ret = AIPU_STATUS_ERROR_JOB_NOT_EXIST;
        goto finish;
    }

    pthread_mutex_lock(&job->lock);
    job->state = (job_state_t)status->state;
    job->pdata = status->pdata;
    if (is_wake_up)
    {
        pthread_cond_signal(&job->cond);
    }
    pthread_mutex_unlock(&job->lock);

finish:
    return ret;
}

aipu_status_t AIRT::Graph::update_job_status(uint32_t job_id, job_state_t state)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    job_desc_t* job = nullptr;

    job = get_job_ptr(job_id);
    if (nullptr == job)
    {
        ret = AIPU_STATUS_ERROR_JOB_NOT_EXIST;
        goto finish;
    }

    pthread_mutex_lock(&job->lock);
    job->state = state;
    pthread_mutex_unlock(&job->lock);

finish:
    return ret;
}

aipu_status_t AIRT::Graph::get_job_status(uint32_t job_id, aipu_job_status_t* status)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    job_desc_t* job = get_job_ptr(job_id);

    if (nullptr == job)
    {
        ret = AIPU_STATUS_ERROR_JOB_NOT_EXIST;
        goto finish;
    }

    if ((JOB_STATE_DONE == job->state) || (JOB_STATE_EXCEPTION == job->state))
    {
        *status = (aipu_job_status_t)job->state;
    }
    else
    {
        *status = AIPU_JOB_STATUS_NO_STATUS;
    }

    if (JOB_STATE_DONE == job->state)
    {
        *status = (aipu_job_status_t)job->state;
    }
    else if (JOB_STATE_EXCEPTION == job->state)
    {
        *status = (aipu_job_status_t)job->state;
        ret = AIPU_STATUS_ERROR_JOB_EXCEPTION;
    }
    else if (JOB_STATE_TIMEOUT == job->state)
    {
        *status = AIPU_JOB_STATUS_NO_STATUS;
        ret = AIPU_STATUS_ERROR_JOB_TIMEOUT;
    }
    else
    {
        *status = AIPU_JOB_STATUS_NO_STATUS;
    }

finish:
    return ret;
}

aipu_status_t AIRT::Graph::clean_job(uint32_t job_id)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    job_desc_t* job = get_job_ptr(job_id);
    tbuf_info_t* tbuf = nullptr;

    if (nullptr == job)
    {
        return AIPU_STATUS_ERROR_JOB_NOT_EXIST;
    }

    if (job->state == JOB_STATE_BUILT)
    {
        ret = AIPU_STATUS_ERROR_JOB_NOT_SCHED;
        goto finish;
    }

    if (job->state == JOB_STATE_SCHED)
    {
        ret = AIPU_STATUS_ERROR_JOB_NOT_END;
        goto finish;
    }

    /* state = done/exception/timeout */
    /* thread buf might be freed before clean_job */
    tbuf = get_tbuf_ptr(job->buf_handle);
    if (nullptr != tbuf)
    {
        tbuf->is_free = true;
    }

    pthread_rwlock_wrlock(&job_queue_lock);
    delete_from_sched_queue_inner(job_id);
    jobs.erase(job->id);
    pthread_mutex_destroy(&job->lock);
    pthread_cond_destroy(&job->cond);
    delete job;
    job = nullptr;
    pthread_rwlock_unlock(&job_queue_lock);

finish:
    return ret;
}

aipu_status_t AIRT::Graph::set_dump_options(uint32_t job_id, const aipu_dump_option_t* option)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    job_desc_t* job = get_job_ptr(job_id);
    int sys_ret = 0;

    if (nullptr == job)
    {
        ret = AIPU_STATUS_ERROR_JOB_NOT_EXIST;
        goto finish;
    }

    if (nullptr == option)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    if (job->state != JOB_STATE_BUILT)
    {
        ret = AIPU_STATUS_ERROR_JOB_SCHED;
        goto finish;
    }

    /**
     * invalid options:
     * 1. use config >= AIPU_DUMP_MAX;
     * 2. do not set dump timing info;
     * 3. set dump output before running;
     * 4. do not set dump what type of data;
     */
    if ((option->flag >= AIPU_DUMP_MAX) ||
        (!(option->flag & (AIPU_DUMP_BEFORE_RUN | AIPU_DUMP_AFTER_RUN |
            AIPU_DUMP_MEM_MAP | AIPU_DUMP_DRV_PROF_DATA))) ||
        (option->flag == (AIPU_DUMP_BEFORE_RUN | AIPU_DUMP_OUT_TENSOR)) ||
        (!(option->flag & (AIPU_DUMP_BEFORE_RUN - 1))))
    {
        ret = AIPU_STATUS_ERROR_INVALID_OPTIONS;
        goto finish;
    }

    /* success */
    job->dump_flag = option->flag;
    if (nullptr == option->fname_suffix)
    {
        job->dump_fname_suffix = "Default";
    }
    else
    {
        job->dump_fname_suffix = option->fname_suffix;
    }
    if (nullptr == option->dir)
    {
        job->dump_dir = ".";
    }
    else
    {
        job->dump_dir = option->dir;
    }

    sys_ret = access(job->dump_dir.c_str(), W_OK);
    if (sys_ret != 0)
    {
        ret = AIPU_STATUS_ERROR_INVALID_PATH;
        goto finish;
    }

finish:
    return ret;
}

aipu_status_t AIRT::Graph::get_debug_info(uint32_t job_id, aipu_debug_info_t* info)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    job_desc_t* job = get_job_ptr(job_id);

    if (nullptr == info)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    if (nullptr == job)
    {
        ret = AIPU_STATUS_ERROR_JOB_NOT_EXIST;
        goto finish;
    }

    info->instruction_base_pa = dev2host(job->config.code.instruction_base_pa);
    info->start_pc_pa = dev2host(job->config.code.start_pc_pa);
    info->interrupt_pc_pa = dev2host(job->config.code.interrupt_pc_pa);

finish:
    return ret;
}