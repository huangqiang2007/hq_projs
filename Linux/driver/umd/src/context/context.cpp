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
 * @file  context.cpp
 * @brief AIPU User Mode Driver (UMD) context module implementation
 */

#include <unistd.h>
#include <string.h>
#include "context.h"
#include "graph/graph.h"
#include "utils/log.h"
#include "utils/debug.h"
#include "utils/helper.h"

AIRT::MainContext::MainContext(): ctrl()
{
    rt_cfg.poll_opt = false;
    pthread_rwlock_init(&gt_lock, NULL);
}

AIRT::MainContext::~MainContext()
{
    pthread_rwlock_destroy(&gt_lock);
}

void AIRT::MainContext::print_graph_header_info(const bin_hdr_t& header) const
{
#if (defined PRINT_GRAPH_HDR_PARSING_INFO) && (PRINT_GRAPH_HDR_PARSING_INFO == 1)
    LOG(LOG_DEFAULT, "Target device: arch %u, version %u, configuration: %u",
        AIPU_ARCH(header.device),
        AIPU_VERSION(header.device),
        AIPU_CONFIG(header.device));
    LOG(LOG_DEFAULT, "Graph version: %u", header.version);
    LOG(LOG_DEFAULT, "Building tool version: major %u, minor %u, build number: %u",
        BUILD_MAJOR(header.build_version),
        BUILD_MINOR(header.build_version),
        BUILD_NUMBER(header.build_version));
    LOG(LOG_DEFAULT, "Graph fsize: %u", header.filesize);
    LOG(LOG_DEFAULT, "Entry: 0x%x", header.entry);
    LOG(LOG_DEFAULT, "Text section: offset 0x%x, size 0x%x", header.text_offset, header.text_size);
    LOG(LOG_DEFAULT, "Rodata section: offset 0x%x, size 0x%x", header.rodata_offset, header.rodata_size);
    LOG(LOG_DEFAULT, "DCR section: offset 0x%x, size 0x%x", header.dcr_offset, header.dcr_size);
    LOG(LOG_DEFAULT, "Data section: offset 0x%x, size 0x%x", header.data_offset, header.data_size);
    LOG(LOG_DEFAULT, "BSS section: offset 0x%x, size 0x%x", header.bss_offset, header.bss_size);
    LOG(LOG_DEFAULT, "Misc section: offset 0x%x, size 0x%x", header.misc_offset, header.misc_size);
#endif
}

void AIRT::MainContext::print_parse_result(const graph_info_t& info, const void* graph) const
{
#if (defined PRINT_GRAPH_PARSING_INFO) && (PRINT_GRAPH_PARSING_INFO == 1)
    LOG(LOG_DEFAULT, "=====================Graph Parse Results====================");
    LOG(LOG_DEFAULT, "Target device: arch %u, version %u, configuration: %u",
        AIPU_ARCH(info.device),
        AIPU_VERSION(info.device),
        AIPU_CONFIG(info.device));
    LOG(LOG_DEFAULT, "Graph version: %u", info.version);
    LOG(LOG_DEFAULT, "Building tool version: major %u, minor %u, build number: %u",
        BUILD_MAJOR(info.build_version),
        BUILD_MINOR(info.build_version),
        BUILD_NUMBER(info.build_version));
    LOG(LOG_DEFAULT, "Graph fsize: %u", info.gbin_size);
    LOG(LOG_DEFAULT, "Entry: 0x%x", info.entry);
    LOG(LOG_DEFAULT, "Process buffer:");
    LOG(LOG_DEFAULT, "--Text: size 0x%x, va 0x%lx (offset 0x%lx)",
        info.pbuf_templ.text_size, (unsigned long)info.pbuf_templ.text_src,
        (unsigned long)info.pbuf_templ.text_src - (unsigned long)graph);
    LOG(LOG_DEFAULT, "--Rodata: size 0x%x, va 0x%lx (offset 0x%lx)",
        info.tbuf_templ.rodata_size, (unsigned long)info.tbuf_templ.rodata_src,
        (unsigned long)info.tbuf_templ.rodata_src - (unsigned long)graph);
    LOG(LOG_DEFAULT, "--Stack: size 0x%x,", info.tbuf_templ.stack_size);
    LOG(LOG_DEFAULT, "--Weight data: size 0x%x, va 0x%lx (offset 0x%lx)",
        info.pbuf_templ.data_size, (unsigned long)info.pbuf_templ.data_src,
        (unsigned long)info.pbuf_templ.data_src - (unsigned long)graph);
    LOG(LOG_DEFAULT, "----in which, %u sections are:", (uint32_t)info.pbuf_templ.static_sections.size());
    for (uint32_t i = 0; i < info.pbuf_templ.static_sections.size(); i++)
    {
        LOG(LOG_DEFAULT, "----section %u: size 0x%x, va 0x%lx (offset = 0x%lx)",
            i, info.pbuf_templ.static_sections[i].size,
            (unsigned long)info.pbuf_templ.static_sections[i].load_src,
            (unsigned long)info.pbuf_templ.static_sections[i].load_src - (unsigned long)graph);
        for (uint32_t j = 0; j < info.pbuf_templ.static_sections[i].sub_sections.size(); j++)
        {
            LOG(LOG_CLOSE, "------sub-section %u: offset_in_section = 0x%x",
                j, info.pbuf_templ.static_sections[i].sub_sections[j].offset_in_section);
            for (uint32_t k = 0; k < info.param_map.size(); k++)
            {
                if ((info.param_map[k].load_type == PARAM_MAP_LOAD_TYPE_STATIC) &&
                    (info.param_map[k].ref_section_iter == i) &&
                    (info.param_map[k].ref_sub_section_iter == j))
                {
                    LOG(LOG_DEFAULT, "--------[section[%u] base pa + 0x%x] =w=> [rodata base pa + 0x%x]",
                        i, info.pbuf_templ.static_sections[i].sub_sections[j].offset_in_section,
                        info.param_map[k].offset_in_map);
                }
            }
        }
    }

    LOG(LOG_DEFAULT, "--Reuse data:");
    LOG(LOG_DEFAULT, "----in which, %u sections are:", (uint32_t)info.tbuf_templ.reuse_sections.size());
    for (uint32_t i = 0; i < info.tbuf_templ.reuse_sections.size(); i++)
    {
        LOG(LOG_DEFAULT, "----section %u: size 0x%x", i, info.tbuf_templ.reuse_sections[i].size);
        for (uint32_t j = 0; j < info.tbuf_templ.reuse_sections[i].sub_sections.size(); j++)
        {
            LOG(LOG_CLOSE, "------sub-section %u: offset_in_section = 0x%x",
                j, info.tbuf_templ.reuse_sections[i].sub_sections[j].offset_in_section);
            for (uint32_t k = 0; k < info.param_map.size(); k++)
            {
                if ((info.param_map[k].load_type == PARAM_MAP_LOAD_TYPE_REUSE) &&
                    (info.param_map[k].ref_section_iter == i) &&
                    (info.param_map[k].ref_sub_section_iter == j))
                {
                    LOG(LOG_DEFAULT, "--------[section[%u] base pa + 0x%x] =w=> [rodata base pa + 0x%x]",
                        i, info.tbuf_templ.reuse_sections[i].sub_sections[j].offset_in_section,
                        info.param_map[k].offset_in_map);
                    break;
                }
            }
        }
    }
    LOG(LOG_DEFAULT, "--Input(s) number: %u", (uint32_t)info.inputs.size());
    for (uint32_t i = 0; i < info.inputs.size(); i++)
    {
        LOG(LOG_DEFAULT, "---Input #%u:", i);
        LOG(LOG_DEFAULT, "------Tensor ID: %u", info.inputs[i].id);
        LOG(LOG_DEFAULT, "------Address: reuse_sections[%u] base pa + 0x%x",
            info.inputs[i].ref_section_iter, info.inputs[i].offset_in_section);
        LOG(LOG_DEFAULT, "------Size: 0x%x", info.inputs[i].size);
    }
    LOG(LOG_DEFAULT, "--Output(s) number: %u", (uint32_t)info.outputs.size());
    for (uint32_t i = 0; i < info.outputs.size(); i++)
    {
        LOG(LOG_DEFAULT, "---Output #%u:", i);
        LOG(LOG_DEFAULT, "------Tensor ID: %u", info.outputs[i].id);
        LOG(LOG_DEFAULT, "------Address: reuse_sections[%u] base pa + 0x%x",
            info.outputs[i].ref_section_iter, info.outputs[i].offset_in_section);
        LOG(LOG_DEFAULT, "------Size: 0x%x", info.outputs[i].size);
    }
    LOG(LOG_DEFAULT, "--Dump(s) number: %u", (uint32_t)info.inter_dumps.size());
    for (uint32_t i = 0; i < info.inter_dumps.size(); i++)
    {
        LOG(LOG_DEFAULT, "---Dump #%u:", i);
        LOG(LOG_DEFAULT, "------Tensor ID: %u", info.inter_dumps[i].id);
        LOG(LOG_DEFAULT, "------Address: reuse_sections[%u] base pa + 0x%x",
            info.inter_dumps[i].ref_section_iter, info.inter_dumps[i].offset_in_section);
        LOG(LOG_DEFAULT, "------Size: 0x%x", info.inter_dumps[i].size);
    }

    LOG(LOG_DEFAULT, "=====================Graph Parse Results====================");
#endif /* PRINT_GRAPH_PARSING_INFO */
}

aipu_status_t AIRT::MainContext::sort_io_tensor(std::vector<io_tensor_desc_t>& tensors) const
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    std::vector<io_tensor_desc_t> tensors_tmp = tensors;

    for (uint32_t i = 0; i < tensors_tmp.size(); i++)
    {
        if (tensors_tmp[i].id >= tensors_tmp.size())
        {
            LOG(LOG_DEBUG, "invalid IO tensor ID too large!");
            ret = AIPU_STATUS_ERROR_INVALID_GBIN;
            break;
        }
        if (tensors_tmp[i].id != i)
        {
            tensors[tensors_tmp[i].id] = tensors_tmp[i];
        }
    }

    return ret;
}

template<typename sub_section_desc_type>
aipu_status_t AIRT::MainContext::fill_io_tensor_desc_inner(uint32_t reuse_sec_iter,
    uint32_t sub_sec_iter, const sub_section_desc_type& sub_section_load, graph_info_t& info) const
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    io_tensor_desc_t io_desc;

    io_desc.id = sub_section_load.id;
    io_desc.ref_section_iter = reuse_sec_iter;
    io_desc.offset_in_section = sub_section_load.offset_in_section_exec;
    io_desc.size = sub_section_load.size;

    /* Do not expose BOOL data type to application */
    io_desc.fmt.data_type = (aipu_data_type_t)(sub_section_load.dtype - 1);
    io_desc.fmt.layout = (aipu_tensor_layout_t)sub_section_load.layout;
    io_desc.fmt.shape.N = sub_section_load.shape[0];
    io_desc.fmt.shape.H = sub_section_load.shape[1];
    io_desc.fmt.shape.W = sub_section_load.shape[2];
    io_desc.fmt.shape.C = sub_section_load.shape[3];

    if (SECTION_TYPE_INPUT == sub_section_load.type)
    {
        info.inputs.push_back(io_desc);
    }
    else if (SECTION_TYPE_OUTPUT == sub_section_load.type)
    {
        info.outputs.push_back(io_desc);
    }
    else if (SECTION_TYPE_INTER_DUMP == sub_section_load.type)
    {
        info.inter_dumps.push_back(io_desc);
    }
    else if (SECTION_TYPE_PROF_DATA == sub_section_load.type)
    {
        info.pdata.push_back(io_desc);
    }
    else if (SECTION_TYPE_PLOG_DATA == sub_section_load.type)
    {
        info.plog_data.push_back(io_desc);
    }

    return ret;
}

template<typename bss_hdr_type, typename static_section_desc_type,
   typename reuse_section_desc_type, typename sub_section_desc_type>
aipu_status_t AIRT::MainContext::parse_bss_section(graph_info_t& info, uint32_t bss_size) const
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    bss_hdr_type bss_header;
    static_section_desc_type static_desc_load;
    reuse_section_desc_type  reuse_desc_load;
    sub_section_desc_type    sub_desc_load;
    linked_list_desc_t       linked_list_load;
    void* desc_load_addr = nullptr;
    section_desc_t section_ir;
    param_map_load_desc_t param;

    void* load_lb = info.bss_src;
    void* load_ub = (void*)((unsigned long)info.bss_src + sizeof(bss_hdr_type) + bss_size);

    /* bss sections (stack/static/reuse descriptors) */
    /* static section cnt non-zero check condition is removed to support no-weight graph */
    memcpy(&bss_header, info.bss_src, sizeof(bss_hdr_type));
    if ((bss_header.stack_size == 0) || (bss_header.stack_align_bytes == 0) ||
        (bss_header.reuse_section_desc_cnt == 0))
    {
        ret = AIPU_STATUS_ERROR_INVALID_GBIN;
        goto finish;
    }

    /* get stack section descriptions */
    info.tbuf_templ.stack_size = bss_header.stack_size;
    info.tbuf_templ.stack_align_in_page = ALIGN_ADDR(bss_header.stack_align_bytes);

    /* static sections (weight/bias) in bss */
    desc_load_addr = (void*)((unsigned long)info.bss_src + sizeof(bss_hdr_type));
    for (uint32_t static_sec_iter = 0; static_sec_iter < bss_header.static_section_desc_cnt; static_sec_iter++)
    {
        section_ir.init();
        if (umd_is_valid_ptr(load_lb, load_ub, desc_load_addr, sizeof(static_section_desc_type)))
        {
            memcpy(&static_desc_load, desc_load_addr, sizeof(static_section_desc_type));
        }
        else
        {
            goto overflow;
        }

        /* update sub-section-desc info. */
        desc_load_addr = (void*)((unsigned long)desc_load_addr + sizeof(static_section_desc_type));
        for (uint32_t sub_sec_iter = 0; sub_sec_iter < static_desc_load.sub_section_cnt; sub_sec_iter++)
        {
            sub_section_desc_t sub_desc_ir;
            if (umd_is_valid_ptr(load_lb, load_ub, desc_load_addr, sizeof(sub_section_desc_type)))
            {
                memcpy(&sub_desc_load, desc_load_addr, sizeof(sub_section_desc_type));
            }
            else
            {
                goto overflow;
            }

            /* get subsection info. */
            sub_desc_ir.offset_in_section = sub_desc_load.offset_in_section_exec;
            section_ir.sub_sections.push_back(sub_desc_ir);

            /* update parameter map element */
            desc_load_addr = (void*)((unsigned long)desc_load_addr + sizeof(sub_section_desc_type));
            for (uint32_t ro_entry_iter = 0; ro_entry_iter < sub_desc_load.offset_in_ro_cnt; ro_entry_iter++)
            {
                uint32_t offset_in_ro = 0;
                if (umd_is_valid_ptr(load_lb, load_ub, desc_load_addr, sizeof(uint32_t)))
                {
                    offset_in_ro = *(uint32_t*)desc_load_addr;
                }
                else
                {
                    goto overflow;
                }
                param.init(offset_in_ro, PARAM_MAP_LOAD_TYPE_STATIC, static_sec_iter, sub_sec_iter,
                    sub_desc_load.offset_in_section_exec, sub_desc_load.addr_mask);
                info.param_map.push_back(param);
                desc_load_addr = (void*)((unsigned long)desc_load_addr + sizeof(uint32_t));
            }
        }

        /* update section descriptor */
        section_ir.size = static_desc_load.size;
        section_ir.align_in_page = ALIGN_ADDR(static_desc_load.align_bytes);
        section_ir.load_src = (void*)((unsigned long)info.pbuf_templ.data_src + static_desc_load.offset_in_file);
        info.pbuf_templ.static_sections.push_back(section_ir);
    }

    /* reuse sections (input/output/intermediate) in bss */
    for (uint32_t reuse_sec_iter = 0; reuse_sec_iter < bss_header.reuse_section_desc_cnt; reuse_sec_iter++)
    {
        section_ir.init();
        if (umd_is_valid_ptr(load_lb, load_ub, desc_load_addr, sizeof(reuse_section_desc_type)))
        {
            memcpy(&reuse_desc_load, desc_load_addr, sizeof(reuse_section_desc_type));
        }
        else
        {
            goto overflow;
        }

        desc_load_addr = (void*)((unsigned long)desc_load_addr + sizeof(reuse_section_desc_type));
        for (uint32_t sub_sec_iter = 0; sub_sec_iter < reuse_desc_load.sub_section_cnt; sub_sec_iter++)
        {
            sub_section_desc_t sub_desc_ir;
            if (umd_is_valid_ptr(load_lb, load_ub, desc_load_addr, sizeof(sub_section_desc_type)))
            {
                memcpy(&sub_desc_load, desc_load_addr, sizeof(sub_section_desc_type));
            }
            else
            {
                goto overflow;
            }

            /* get io tensor info if this sub-section represents io */
            if ((SECTION_TYPE_INPUT == sub_desc_load.type) ||
                (SECTION_TYPE_OUTPUT == sub_desc_load.type) ||
                (SECTION_TYPE_INTER_DUMP == sub_desc_load.type) ||
                (SECTION_TYPE_PROF_DATA == sub_desc_load.type) ||
                (SECTION_TYPE_PLOG_DATA == sub_desc_load.type))
            {
                fill_io_tensor_desc_inner<sub_section_desc_type>(reuse_sec_iter,
                    sub_sec_iter, sub_desc_load, info);
            }

            /* get subsection info. */
            sub_desc_ir.offset_in_section = sub_desc_load.offset_in_section_exec;
            section_ir.sub_sections.push_back(sub_desc_ir);

            /* update parameter map element */
            desc_load_addr = (void*)((unsigned long)desc_load_addr + sizeof(sub_section_desc_type));
            for (uint32_t ro_entry_iter = 0; ro_entry_iter < sub_desc_load.offset_in_ro_cnt; ro_entry_iter++)
            {
                uint32_t offset_in_ro = 0;
                if (umd_is_valid_ptr(load_lb, load_ub, desc_load_addr, sizeof(uint32_t)))
                {
                    offset_in_ro = *(uint32_t*)desc_load_addr;
                }
                else
                {
                    goto overflow;
                }
                param.init(offset_in_ro, PARAM_MAP_LOAD_TYPE_REUSE, reuse_sec_iter, sub_sec_iter,
                    sub_desc_load.offset_in_section_exec, sub_desc_load.addr_mask);
                info.param_map.push_back(param);
                desc_load_addr = (void*)((unsigned long)desc_load_addr + sizeof(uint32_t));
            }
        }

        /* update section descriptor */
        section_ir.load_src = nullptr;
        section_ir.align_in_page = ALIGN_ADDR(reuse_desc_load.align_bytes);
        section_ir.size = reuse_desc_load.size;
        info.tbuf_templ.reuse_sections.push_back(section_ir);
    }

    /* linked list section descriptor in bss */
    if ((AIPU_LOADABLE_GRAPH_V0004 == GVERSION_MINOR(info.version)) && (info.linked_list_flag))
    {
        if (umd_is_valid_ptr(load_lb, load_ub, desc_load_addr, sizeof(linked_list_desc_t)))
        {
            memcpy(&linked_list_load, desc_load_addr, sizeof(linked_list_desc_t));
            if (linked_list_load.node_cnt == 0)
            {
                ret = AIPU_STATUS_ERROR_INVALID_GBIN;
                goto finish;
            }
            desc_load_addr = (void*)((unsigned long)desc_load_addr + sizeof(linked_list_desc_t));
        }
        else
        {
            goto overflow;
        }

        for (uint32_t i = 0; i < linked_list_load.node_cnt; i++)
        {
            list_node_desc_t node;
            memcpy(&node, desc_load_addr, sizeof(list_node_desc_t));
            info.dcr_map.push_back((dcr_map_load_desc_t)node);
            desc_load_addr = (void*)((unsigned long)desc_load_addr + sizeof(list_node_desc_t));
        }
    }

    /* sort IO tensors by tensor ID */
    if (sort_io_tensor(info.outputs) || sort_io_tensor(info.inputs))
    {
        goto finish;
    }

    /* success */
    goto finish;

overflow:
    LOG(LOG_DEBUG, "[UMD ERROR] Input graph binary contains invalid offset value(s)!");
    ret = AIPU_STATUS_ERROR_INVALID_GBIN;

finish:
    return ret;
}

aipu_status_t AIRT::MainContext::parse_graph_header(const void* graph, uint32_t size,
    graph_info_t& info, uint32_t& bss_size) const
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    bin_hdr_t header;

    memcpy(&header, graph, BIN_HDR_SIZE);
    print_graph_header_info(header);

    if (strcmp(header.ident, MAGIC) != 0)
    {
        ret = AIPU_STATUS_ERROR_INVALID_GBIN;
        goto finish;
    }

    if (size < header.filesize)
    {
        ret = AIPU_STATUS_ERROR_INVALID_GBIN;
        goto finish;
    }

    if ((AIPU_LOADABLE_GRAPH_V0003 != GVERSION_MINOR(header.version)) &&
        (AIPU_LOADABLE_GRAPH_V0004 != GVERSION_MINOR(header.version)))
    {
        ret = AIPU_STATUS_ERROR_GVERSION_UNSUPPORTED;
        goto finish;
    }

    if (header.hdrsize != BIN_HDR_SIZE)
    {
        ret = AIPU_STATUS_ERROR_INVALID_GBIN;
        goto finish;
    }

    if (header.entry >= header.text_size)
    {
        ret = AIPU_STATUS_ERROR_INVALID_GBIN;
        goto finish;
    }

    if ((header.text_offset < BIN_HDR_SIZE) ||
        ((header.text_offset + header.text_size) > header.rodata_offset) ||
        (header.text_size == 0))
    {
        ret = AIPU_STATUS_ERROR_INVALID_GBIN;
        goto finish;
    }

    if (((header.rodata_offset + header.rodata_size) > header.data_offset) ||
        (header.rodata_size == 0))
    {
        ret = AIPU_STATUS_ERROR_INVALID_GBIN;
        goto finish;
    }

    /* data size non-zero check condition is removed to support no-weight graph */
    if (((header.data_offset + header.data_size) > header.bss_offset))
    {
        ret = AIPU_STATUS_ERROR_INVALID_GBIN;
        goto finish;
    }

    if (((header.bss_offset + header.bss_size) > header.filesize) ||
        (header.bss_size == 0))
    {
        ret = AIPU_STATUS_ERROR_INVALID_GBIN;
        goto finish;
    }

    /* success */
    info.gbin = (void*)graph;
    info.gbin_size = size;
    info.device = header.device;
    info.version = header.version;
    info.build_version = header.build_version;
    info.entry = header.entry;
    info.asid_flag = GET_ASID_FLAG(header.flag);
    info.bss_src = (void*)((unsigned long)graph + header.bss_offset);
    info.pbuf_templ.text_src = (void*)((unsigned long)graph + header.text_offset);
    info.pbuf_templ.text_size = header.text_size;
    info.pbuf_templ.data_src = (void*)((unsigned long)graph + header.data_offset);
    info.pbuf_templ.data_size = header.data_size;
    info.tbuf_templ.rodata_src = (void*)((unsigned long)graph + header.rodata_offset);
    info.tbuf_templ.rodata_size = header.rodata_size;
    if (header.dcr_size != 0)
    {
        info.tbuf_templ.dcr_src = (void*)((unsigned long)graph + header.dcr_offset);
    }
    else
    {
        info.tbuf_templ.dcr_src = nullptr;
    }
    info.tbuf_templ.dcr_size = header.dcr_size;

    if (AIPU_LOADABLE_GRAPH_V0004 == GVERSION_MINOR(header.version))
    {
        info.linked_list_flag = GET_LINKED_LIST_FLAG(header.flag);
    }
    else
    {
        info.linked_list_flag = 0;
    }

    bss_size = header.bss_size;

finish:
    return ret;
}

aipu_status_t AIRT::MainContext::parse_graph(const void* graph, uint32_t size, graph_info_t& info) const
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    uint32_t bss_size = 0;

    if (nullptr == graph)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    if (size < BIN_HDR_SIZE)
    {
        ret = AIPU_STATUS_ERROR_INVALID_SIZE;
        goto finish;
    }

    ret = parse_graph_header(graph, size, info, bss_size);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }

    if ((AIPU_LOADABLE_GRAPH_V0003 == GVERSION_MINOR(info.version)) ||
        (AIPU_LOADABLE_GRAPH_V0004 == GVERSION_MINOR(info.version)))
    {
        ret = parse_bss_section<bss_hdr_v3_t, static_section_desc_v3_t,
                reuse_section_desc_v3_t, sub_section_desc_v3_t>(info, bss_size);
        if (AIPU_STATUS_SUCCESS != ret)
        {
            goto finish;
        }
    }
    else
    {
        ret = AIPU_STATUS_ERROR_GVERSION_UNSUPPORTED;
        goto finish;
    }

    if (!ctrl.match_target_dev(AIPU_ARCH(info.device),
        AIPU_VERSION(info.device), AIPU_CONFIG(info.device)))
    {
        if (rt_cfg.bypass_version_check)
        {
            LOG(LOG_WARN, "the version of the loading binary does not match with target AIPU!");
        }
        else
        {
            ret = AIPU_STATUS_ERROR_TARGET_NOT_FOUND;
        }
        goto finish;
    }

finish:
    return ret;
}

uint32_t AIRT::MainContext::create_unique_graph_id_inner() const
{
    uint32_t id_candidate = 1;
    while (graphs.count(id_candidate) == 1)
    {
        id_candidate++;
    }
    return id_candidate;
}

aipu_status_t AIRT::MainContext::create_graph_object(const graph_info_t& info, bool map_flag,
    Graph** gobj, uint32_t id)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    Graph* p_gobj = nullptr;

    if (nullptr == gobj)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    /* assumed that info is a valid one returned by parse_graph() */
    p_gobj = new Graph(id, ctrl);
    ret = p_gobj->load(info, map_flag);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        destroy_graph_object(&p_gobj);
    }

    /* success or return nullptr */
    *gobj = p_gobj;

finish:
    return ret;
}

AIRT::Graph* AIRT::MainContext::get_graph_object(uint32_t id)
{
    Graph* p_gobj = nullptr;
    pthread_rwlock_rdlock(&gt_lock);
    if (0 != graphs.count(id))
    {
        p_gobj = graphs[id];
    }
    pthread_rwlock_unlock(&gt_lock);
    return p_gobj;
}

aipu_status_t AIRT::MainContext::destroy_graph_object(Graph** gobj)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;

    if ((nullptr == gobj) || (nullptr == (*gobj)))
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    ret = (*gobj)->unload();
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }

    /* success */
    delete *gobj;
    *gobj = nullptr;

finish:
    return ret;
}

bool AIRT::MainContext::is_deinit_ok()
{
    bool ret = true;
    GraphTable::const_iterator iter;
    pthread_rwlock_rdlock(&gt_lock);
    for (iter = graphs.begin(); iter != graphs.end(); iter++)
    {
        if (!iter->second->is_unload_ok())
        {
            ret = false;
            break;
        }
    }
    pthread_rwlock_unlock(&gt_lock);
    return ret;
}

aipu_status_t AIRT::MainContext::init()
{
    return ctrl.init();
}

void AIRT::MainContext::force_deinit()
{
    GraphTable::iterator iter;
    pthread_rwlock_wrlock(&gt_lock);
    for (iter = graphs.begin(); iter != graphs.end(); iter++)
    {
        iter->second->unload();
    }
    graphs.clear();
    pthread_rwlock_unlock(&gt_lock);
    ctrl.deinit();
}

aipu_status_t AIRT::MainContext::deinit()
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;

    if (!is_deinit_ok())
    {
        ret = AIPU_STATUS_ERROR_DEINIT_FAIL;
        goto finish;
    }

    force_deinit();

finish:
    return ret;
}

aipu_status_t AIRT::MainContext::config_simulation(const aipu_simulation_config_t* config)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
#if (defined X86_LINUX) && (X86_LINUX==1)
    ret = ctrl.config_simulation(config);
#else
    ret = AIPU_STATUS_ERROR_OP_NOT_SUPPORTED;
#endif
    return ret;
}

aipu_status_t AIRT::MainContext::set_runtime_config(const aipu_runtime_config_t* config)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;

    if (nullptr == config)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    rt_cfg = *config;

finish:
    return ret;
}

aipu_status_t AIRT::MainContext::load_graph(const void* graph, uint32_t size, bool map_flag,
    aipu_graph_desc_t* gdesc)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    graph_info_t info;
    Graph* gobj = nullptr;
    uint32_t id = 0;

    if (nullptr == gdesc)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    ret = parse_graph(graph, size, info);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }

    print_parse_result(info, graph);

    pthread_rwlock_wrlock(&gt_lock);
    id = create_unique_graph_id_inner();
    graphs[id] = nullptr;
    pthread_rwlock_unlock(&gt_lock);

    ret = create_graph_object(info, map_flag, &gobj, id);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        pthread_rwlock_wrlock(&gt_lock);
        graphs.erase(id);
        pthread_rwlock_unlock(&gt_lock);
        goto finish;
    }

    /* success */
    pthread_rwlock_wrlock(&gt_lock);
    graphs[id] = gobj;
    pthread_rwlock_unlock(&gt_lock);
    gobj->get_graph_desc(gdesc);

finish:
    return ret;
}

aipu_status_t AIRT::MainContext::unload_graph(const aipu_graph_desc_t* gdesc)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    Graph* p_gobj = nullptr;

    if (nullptr == gdesc)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    p_gobj = get_graph_object(gdesc->id);
    if (nullptr == p_gobj)
    {
        ret = AIPU_STATUS_ERROR_GRAPH_NOT_EXIST;
        goto finish;
    }

    ret = destroy_graph_object(&p_gobj);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }

    /* p_gobj becomes NULL after destroy */

    /* success */
    pthread_rwlock_wrlock(&gt_lock);
    graphs.erase(gdesc->id);
    pthread_rwlock_unlock(&gt_lock);

finish:
    return ret;
}

aipu_status_t AIRT::MainContext::alloc_tensor_buffers(const aipu_graph_desc_t* gdesc,
    aipu_buffer_alloc_info_t* info)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    Graph* p_gobj = nullptr;

    if ((nullptr == gdesc) || (nullptr == info))
    {
        return AIPU_STATUS_ERROR_NULL_PTR;
    }

    p_gobj = get_graph_object(gdesc->id);
    if (nullptr == p_gobj)
    {
        ret = AIPU_STATUS_ERROR_GRAPH_NOT_EXIST;
        goto finish;
    }

    ret = p_gobj->alloc_thread_buffer(info);

finish:
    return ret;
}

aipu_status_t AIRT::MainContext::free_tensor_buffers(uint32_t handle)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    Graph* p_gobj = get_graph_object(Graph::handle2graph_id(handle));
    if (nullptr == p_gobj)
    {
        ret = AIPU_STATUS_ERROR_INVALID_HANDLE;
        goto finish;
    }

    ret = p_gobj->free_thread_buffer(handle);

finish:
    return ret;
}

aipu_status_t AIRT::MainContext::create_new_job(const aipu_graph_desc_t* gdesc,
    uint32_t handle, uint32_t* job_id)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    Graph* p_gobj = nullptr;

    if ((nullptr == gdesc) || (nullptr == job_id))
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    p_gobj = get_graph_object(gdesc->id);
    if (nullptr == p_gobj)
    {
        ret = AIPU_STATUS_ERROR_GRAPH_NOT_EXIST;
        goto finish;
    }

    ret = p_gobj->build_new_job(handle, job_id);

finish:
    return ret;
}

aipu_status_t AIRT::MainContext::flush_job(uint32_t job_id)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    Graph* p_gobj = get_graph_object(Graph::job_id2graph_id(job_id));
    if (nullptr == p_gobj)
    {
        ret = AIPU_STATUS_ERROR_JOB_NOT_EXIST;
        goto finish;
    }

    ret = p_gobj->flush_job(job_id);

finish:
    return ret;
}

aipu_status_t AIRT::MainContext::wait_for_job_end(uint32_t job_id, int32_t time_out,
    aipu_job_status_t* status)
{
    aipu_status_t ret = AIPU_STATUS_ERROR_JOB_NOT_EXIST;
    std::vector<job_status_desc> jobs_status;
    Graph* p_gobj = nullptr;

    if (nullptr == status)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    if (time_out <= 0)
    {
        time_out = -1;
    }

    p_gobj = get_graph_object(Graph::job_id2graph_id(job_id));
    if (nullptr == p_gobj)
    {
        ret = AIPU_STATUS_ERROR_JOB_NOT_EXIST;
        goto error;
    }

    ret = p_gobj->is_job_sched(job_id);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto error;
    }

    if (p_gobj->is_job_end(job_id))
    {
        ret = p_gobj->get_job_status(job_id, status);
        if (AIPU_STATUS_SUCCESS != ret)
        {
            goto error;
        }
        goto dump;
    }

#if (defined ARM_LINUX) && (ARM_LINUX==1)
    if (rt_cfg.poll_opt)
    {
        ret = p_gobj->wait_for_job_end_sleep(job_id, time_out, status);
        if (AIPU_STATUS_SUCCESS != ret)
        {
            goto error;
        }
    }
    else
    {
        ret = ctrl.poll_status(jobs_status, 1, time_out, 1, job_id);
        if (AIPU_STATUS_SUCCESS != ret)
        {
            goto error;
        }

        if (jobs_status.size() == 0)
        {
            ctrl.kill_timeout_job(job_id);
            p_gobj->update_job_status(job_id, JOB_STATE_TIMEOUT);
            ret = AIPU_STATUS_ERROR_JOB_TIMEOUT;
            goto error;
        }

        ret = p_gobj->update_job_status(&jobs_status[0], 0);
        if (AIPU_STATUS_SUCCESS != ret)
        {
            goto error;
        }

        *status = (aipu_job_status_t)jobs_status[0].state;
    }
#endif

dump:
    /* success */
    p_gobj->dump_end_job_buffers(job_id);
    goto finish;

error:
    *status = AIPU_JOB_STATUS_NO_STATUS;

finish:
    return ret;
}

uint32_t AIRT::MainContext::get_max_poll_job_cnt()
{
    uint32_t cnt = 1;
    GraphTable::const_iterator g_iter;

    pthread_rwlock_rdlock(&gt_lock);
    for (g_iter = graphs.begin(); g_iter != graphs.end(); g_iter++)
    {
        if (g_iter->second != nullptr)
        {
            cnt += g_iter->second->get_sched_job_cnt();
        }
    }
    pthread_rwlock_unlock(&gt_lock);

    return cnt;
}

aipu_status_t AIRT::MainContext::poll_job_status(uint32_t* job_cnt, int32_t time_out)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    std::vector<job_status_desc> jobs_status;
    Graph* p_gobj = nullptr;

    if (nullptr == job_cnt)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    if (time_out <= 0)
    {
        time_out = -1;
    }

    ret = ctrl.poll_status(jobs_status, get_max_poll_job_cnt(), time_out);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }

    LOG(LOG_CLOSE, "get done job status number: %u.", (uint32_t)jobs_status.size());

    for (uint32_t i = 0; i < jobs_status.size(); i++)
    {
        p_gobj = get_graph_object(Graph::job_id2graph_id(jobs_status[i].job_id));
        if (nullptr == p_gobj)
        {
            ret = AIPU_STATUS_ERROR_JOB_NOT_EXIST;
            goto finish;
        }
        ret = p_gobj->update_job_status(&jobs_status[i], 1);
        if (AIPU_STATUS_SUCCESS != ret)
        {
            goto finish;
        }
    }

    *job_cnt = jobs_status.size();

finish:
    return ret;
}

aipu_status_t AIRT::MainContext::clean_job(uint32_t job_id)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    Graph* p_gobj = get_graph_object(Graph::job_id2graph_id(job_id));
    if (nullptr == p_gobj)
    {
        ret = AIPU_STATUS_ERROR_JOB_NOT_EXIST;
        goto finish;
    }

    ret = p_gobj->clean_job(job_id);

finish:
    return ret;
}

aipu_status_t AIRT::MainContext::set_dump_options(uint32_t job_id, const aipu_dump_option_t* option)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    Graph* p_gobj = get_graph_object(Graph::job_id2graph_id(job_id));
    if (nullptr == p_gobj)
    {
        ret = AIPU_STATUS_ERROR_JOB_NOT_EXIST;
        goto finish;
    }

    ret = p_gobj->set_dump_options(job_id, option);

finish:
    return ret;
}

aipu_status_t AIRT::MainContext::get_debug_info(uint32_t job_id, aipu_debug_info_t* info)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    Graph* p_gobj = get_graph_object(Graph::job_id2graph_id(job_id));
    if (nullptr == p_gobj)
    {
        ret = AIPU_STATUS_ERROR_JOB_NOT_EXIST;
        goto finish;
    }

    ret = p_gobj->get_debug_info(job_id, info);

finish:
    return ret;
}

aipu_status_t AIRT::MainContext::get_dev_status(uint32_t* value) const
{
    return ctrl.get_dev_status(value);
}

aipu_status_t AIRT::MainContext::get_status_msg(aipu_status_t status, const char** msg)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;

    if (nullptr == msg)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    *msg = umd_status_string[status];

finish:
    return ret;
}
