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
 * @file  device_ctrl.cpp
 * @brief AIPU User Mode Driver (UMD) device control module implementation
 */

#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include "device_ctrl.h"
#include "graph/common.h"
#include "utils/log.h"
#include "utils/helper.h"

extern aipu_arch_t arch[AIPU_ARCH_CONFIG_MAX];

AIRT::DeviceCtrl::DeviceCtrl()
{
#if (defined X86_LINUX) && (X86_LINUX==1)
    char* sim_version = NULL;
#endif
    fd = 0;
    host_aipu_shm_offset = 0;
#if (defined X86_LINUX) && (X86_LINUX==1)
    init_aipu_arch();
    has_additional_opt = 0;
    pthread_mutex_init(&glock, NULL);

    sim_version = getenv("AIPU_SIM_VERSION");
    if ((sim_version != NULL) && (!strcmp(sim_version, "ZHOUYI_V1_SIM_OLD_ARCH")))
    {
        use_new_arch_sim = 0;
    }
    else
    {
        use_new_arch_sim = 1;
    }
#endif
}

AIRT::DeviceCtrl::~DeviceCtrl()
{
#if (defined X86_LINUX) && (X86_LINUX==1)
    pthread_mutex_destroy(&glock);
#endif
}

#if (defined X86_LINUX) && (X86_LINUX==1)
aipu_status_t AIRT::DeviceCtrl::create_simulation_input_file(uint32_t graph_id, const buffer_desc_t& desc,
    char* fname, const char* interfix, uint32_t load_size)
{
    snprintf(fname, FNAME_LEN, "%s/Simulation_Graph0x%x_%s_Base0x%lx_Size0x%x.bin",
        output_dir.c_str(), graph_id, interfix, desc.pa - host_aipu_shm_offset, load_size);
    return umd_dump_file_helper(fname, (const void*)desc.va, load_size);
}
#endif

#if (defined X86_LINUX) && (X86_LINUX==1)
uint32_t AIRT::DeviceCtrl::get_sim_data_size(const pbuf_alloc_templ_t& pbuf_templ,
    const tbuf_alloc_templ_t& tbuf_templ) const
{
    uint32_t tot_size = tbuf_templ.stack_size;
    for (uint32_t stensor_iter = 0; stensor_iter < pbuf_templ.static_sections.size(); stensor_iter++)
    {
        tot_size += ALIGN_PAGE(pbuf_templ.static_sections[stensor_iter].size);
    }
    for (uint32_t reuse_iter = 0; reuse_iter < tbuf_templ.reuse_sections.size(); reuse_iter++)
    {
        tot_size += ALIGN_PAGE(tbuf_templ.reuse_sections[reuse_iter].size);
    }
    return tot_size;
}
#endif

#if (defined X86_LINUX) && (X86_LINUX==1)
aipu_status_t AIRT::DeviceCtrl::update_z1_simulation_sys_cfg(uint32_t config, FILE* fp) const
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    char cfg_item[1024];
    std::map<uint32_t, aipu_arch_t>::const_iterator iter = aipu_arch.find(config);
    if (iter == aipu_arch.end())
    {
        ret = AIPU_STATUS_ERROR_TARGET_NOT_FOUND;
        goto finish;
    }

    snprintf(cfg_item, sizeof(cfg_item), "SYS_INFO=%s\n", iter->second.sys_info);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "VREG_NUM=%u\n", iter->second.vreg_num);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "LSRAM_SIZE=%u\n", iter->second.lsram_size_kb);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "LSRAM_BANK=%u\n", iter->second.lsram_bank);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "GSRAM_SIZE=%u\n", iter->second.gsram_size_kb);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "GSRAM_BANK=%u\n", iter->second.gsram_bank);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "DMA_CHAN=%u\n", iter->second.dma_chan);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "HAS_IRAM=%s\n", iter->second.has_iram);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "HAS_DCACHE=%s\n", iter->second.has_dcache);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "ICACHE_SIZE=%u\n", iter->second.icache_size);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "HWA_MAC=%u\n", iter->second.hwa_mac);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "TEC_NUM=%u\n", iter->second.tec_num);
    fputs(cfg_item, fp);

finish:
    return ret;
}
#endif

#if (defined X86_LINUX) && (X86_LINUX==1)
aipu_status_t AIRT::DeviceCtrl::update_simulation_rtcfg_z1_old_arch(simulation_res_t& sim, const dev_config_t& config,
    FILE* fp, char* cfg_fname)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    char cfg_item[OPT_LEN];
    char text_bin[FNAME_LEN];
    uint32_t min_pa_odata_base = 0xFFFFFFFF;
    void* min_pa_odata_va = nullptr;
    uint32_t max_pa_odata_base = 0;
    uint32_t max_pa_odata_size = 0;

    if ((fp == NULL) || (cfg_fname == NULL))
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    ret = update_z1_simulation_sys_cfg(config.hw_config, fp);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }

    snprintf(text_bin, sizeof(text_bin), "%s",
        sim.code_fname);
    snprintf(cfg_item, sizeof(cfg_item), "instr_base=0x%x\n",
        (uint32_t)config.code.instruction_base_pa);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "start_pc=0x%x\n",
        (uint32_t)config.code.start_pc_pa);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "idata=%s@0x%x\n",
        sim.rodata_fname, (uint32_t)config.rodata_base);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "idata2=%s@0x%x\n",
        sim.data_fname, (uint32_t)config.stack_base);
    fputs(cfg_item, fp);

    for (uint32_t i = 0; i < sim.outputs.size(); i++)
    {
        if (sim.outputs[i].pa < min_pa_odata_base)
        {
            min_pa_odata_base = sim.outputs[i].pa;
            min_pa_odata_va = sim.outputs[i].va;
        }
        if (sim.outputs[i].pa > max_pa_odata_base)
        {
            max_pa_odata_base = sim.outputs[i].pa;
            max_pa_odata_size = sim.outputs[i].size;
        }
    }

    sim.odata_whole_pa = min_pa_odata_base;
    sim.odata_whole_va = min_pa_odata_va;
    sim.odata_whole_size = max_pa_odata_base + max_pa_odata_size - min_pa_odata_base;
    snprintf(sim.odata_fname, sizeof(sim.odata_fname),
        "%s/Simulation_Graph0x%x_Output_Base0x%x_Size0x%x.bin",
        output_dir.c_str(), sim.graph_id, (uint32_t)sim.odata_whole_pa, sim.odata_whole_size);

    snprintf(cfg_item, sizeof(cfg_item), "odata=%s@0x%x\n",
        sim.odata_fname, sim.odata_whole_pa);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "odata_size=0x%x\n", sim.odata_whole_size);
    fputs(cfg_item, fp);
    fclose(fp);

    snprintf(simulation_cmd, sizeof(simulation_cmd), "./%s %s --sys_config=%s",
        simulator.c_str(), text_bin, cfg_fname);
    if (has_additional_opt)
    {
        strcat(simulation_cmd, " ");
        strcat(simulation_cmd, additional_opt.c_str());
    }

finish:
    return ret;
}
#endif

#if (defined X86_LINUX) && (X86_LINUX==1)
aipu_status_t AIRT::DeviceCtrl::update_simulation_rtcfg_new_arch(simulation_res_t& sim, const dev_config_t& config,
    FILE* fp, char* cfg_fname)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    char cfg_item[OPT_LEN];

    if ((fp == NULL) || (cfg_fname == NULL))
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    snprintf(cfg_item, sizeof(cfg_item), "CONFIG=Z%d-%04d\n", config.hw_version, config.hw_config);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "LOG_LEVEL=0\n");
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "LOG_FILE=log_default\n");
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "FAST_FWD_INST=0\n");
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "INPUT_INST_CNT=1\n");
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "INPUT_INST_FILE0=%s\n", sim.code_fname);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "INPUT_INST_BASE0=0x%x\n", (uint32_t)config.code.instruction_base_pa);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "INPUT_INST_STARTPC0=0x%x\n", (uint32_t)config.code.start_pc_pa);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "INT_PC=0x%x\n", (uint32_t)config.code.interrupt_pc_pa);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "INPUT_DATA_CNT=2\n");
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "INPUT_DATA_FILE0=%s\n", sim.rodata_fname);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "INPUT_DATA_BASE0=0x%x\n", (uint32_t)config.rodata_base);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "INPUT_DATA_FILE1=%s\n", sim.data_fname);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "INPUT_DATA_BASE1=0x%x\n", (uint32_t)config.stack_base);
    fputs(cfg_item, fp);
    snprintf(cfg_item, sizeof(cfg_item), "OUTPUT_DATA_CNT=%u\n", (uint32_t)sim.outputs.size());
    fputs(cfg_item, fp);
    for (uint32_t i = 0; i < sim.outputs.size(); i++)
    {
        snprintf(cfg_item, sizeof(cfg_item), "OUTPUT_DATA_FILE%u=%s\n", i, sim.outputs[i].fname);
        fputs(cfg_item, fp);
        snprintf(cfg_item, sizeof(cfg_item), "OUTPUT_DATA_BASE%u=0x%x\n", i, sim.outputs[i].pa);
        fputs(cfg_item, fp);
        snprintf(cfg_item, sizeof(cfg_item), "OUTPUT_DATA_SIZE%u=0x%x\n", i, sim.outputs[i].size);
        fputs(cfg_item, fp);
    }
    snprintf(cfg_item, sizeof(cfg_item), "RUN_DESCRIPTOR=BIN[0]\n");
    fputs(cfg_item, fp);
    fclose(fp);

    snprintf(simulation_cmd, sizeof(simulation_cmd), "%s %s", simulator.c_str(), cfg_fname);

finish:
    return ret;
}
#endif

#if (defined X86_LINUX) && (X86_LINUX==1)
aipu_status_t AIRT::DeviceCtrl::update_simulation_rtcfg(uint32_t graph_id, const dev_config_t& config)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    FILE* fp = NULL;
    char cfg_fname[512];
    std::map<uint32_t, simulation_res_t>::iterator giter = graphs.find(graph_id);

    if (graphs.end() == giter)
    {
        ret = AIPU_STATUS_ERROR_GRAPH_NOT_EXIST;
        goto finish;
    }

    /* create rodata file */
    ret = create_simulation_input_file(graph_id, giter->second.rodata, giter->second.rodata_fname,
        "Idata0", giter->second.rodata.size);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        goto finish;
    }

    /* create data file */
    ret = create_simulation_input_file(graph_id, giter->second.data, giter->second.data_fname,
        "Idata1", giter->second.data.size);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        goto finish;
    }

    /* create config file */
    snprintf(cfg_fname, sizeof(cfg_fname), "%sruntime.cfg", cfg_file_dir.c_str());
    fp = fopen(cfg_fname, "w");
    if (NULL == fp)
    {
        ret = AIPU_STATUS_ERROR_OPEN_FILE_FAIL;
        LOG(LOG_ERR, "Create config file failed: %s!", cfg_fname);
        goto finish;
    }

    if ((config.arch == AIPU_HW_ARCH_ZHOUYI) && (config.hw_version == AIPU_HW_VERSION_ZHOUYI_V1) &&
        (use_new_arch_sim == 0))
    {
        ret = update_simulation_rtcfg_z1_old_arch(giter->second, config, fp, cfg_fname);
    }
    else
    {
        ret = update_simulation_rtcfg_new_arch(giter->second, config, fp, cfg_fname);
    }

finish:
    return ret;
}
#endif

#if (defined X86_LINUX) && (X86_LINUX==1)
void AIRT::DeviceCtrl::init_aipu_arch()
{
    aipu_arch[AIPU_CONFIG_0904] = arch[0];
    aipu_arch[AIPU_CONFIG_1002] = arch[1];
    aipu_arch[AIPU_CONFIG_0701] = arch[2];
}
#endif

#if (defined X86_LINUX) && (X86_LINUX==1)
aipu_status_t AIRT::DeviceCtrl::config_simulation(const aipu_simulation_config_t* config)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    int sys_ret = 0;

    if ((nullptr == config) ||
        (nullptr == config->simulator) ||
        (nullptr == config->cfg_file_dir) ||
        (nullptr == config->output_dir))
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    sys_ret = access(config->simulator, X_OK) |
              access(config->cfg_file_dir, W_OK) |
              access(config->output_dir, W_OK);
    if (sys_ret != 0)
    {
        ret = AIPU_STATUS_ERROR_INVALID_PATH;
        goto finish;
    }

    simulator = config->simulator;
    cfg_file_dir = config->cfg_file_dir;
    output_dir = config->output_dir;
    if (config->simulator_opt != NULL)
    {
        has_additional_opt = true;
        additional_opt = config->simulator_opt;
    }

finish:
    return ret;
}
#endif

uint32_t get_aligned_addr(uint32_t base, uint32_t align_in_page)
{
    uint32_t n = 0;
    while((n * align_in_page * 4096) < base)
    {
        n++;
    }
    return n * align_in_page * 4096;
}

aipu_status_t AIRT::DeviceCtrl::alloc_rodata_buffer(uint32_t region_id,
        const tbuf_alloc_templ_t& tbuf_templ, buffer_desc_t& rodata, buffer_desc_t& descriptor)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    uint32_t dtype = 0;

#if (defined ARM_LINUX) && (ARM_LINUX==1)
    dtype = AIPU_MM_DATA_TYPE_RO_STACK;
#endif

    ret = malloc_buf(dtype, tbuf_templ.rodata_size + tbuf_templ.dcr_size, 1,
        &rodata, region_id);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }

    descriptor.size = tbuf_templ.dcr_size;
    descriptor.real_size = tbuf_templ.dcr_size;
    if (descriptor.size != 0)
    {
        descriptor.va = (void*)((unsigned long)rodata.va + tbuf_templ.rodata_size);
        descriptor.pa = rodata.pa + tbuf_templ.rodata_size;
        descriptor.region_id = region_id;
    }
    else
    {
        descriptor.va = nullptr;
        descriptor.pa = 0;
        descriptor.region_id = 0;
    }

finish:
    return ret;
}

#if (defined X86_LINUX) && (X86_LINUX==1)
aipu_status_t AIRT::DeviceCtrl::simulation_alloc_data_buffer(uint32_t graph_id, const pbuf_alloc_templ_t& pbuf_templ,
    pbuf_info_t& pbuf, const tbuf_alloc_templ_t& tbuf_templ, tbuf_info_t& tbuf)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    uint32_t offset = 0;
    uint32_t data_phys_base = 0;
    uint32_t data_buf_alloc_size = 0;
    buffer_desc_t buf;
    std::map<uint32_t, simulation_res_t>::iterator giter = graphs.find(graph_id);
    if (giter == graphs.end())
    {
        ret = AIPU_STATUS_ERROR_GRAPH_NOT_EXIST;
        goto finish;
    }

    /* rodata */
    ret = alloc_rodata_buffer(pbuf.text.region_id, tbuf_templ, tbuf.rodata, tbuf.descriptor);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }
    giter->second.rodata = tbuf.rodata;

    /* alloc a whole contiguous data buffer (contains stack, weight, reuse) */
    /* alloc device pa */
    /* stack */
    tbuf.stack.pa = get_aligned_addr(simulation_malloc_top, tbuf_templ.stack_align_in_page);
    tbuf.stack.size = ALIGN_PAGE(tbuf_templ.stack_size);
    tbuf.stack.real_size = tbuf_templ.stack_size;
    simulation_malloc_top = tbuf.stack.pa + tbuf.stack.size;
    data_phys_base = tbuf.stack.pa;

    /* static data */
    for (uint32_t stensor_iter = 0; stensor_iter < pbuf_templ.static_sections.size(); stensor_iter++)
    {
        buf.pa = get_aligned_addr(simulation_malloc_top,
            pbuf_templ.static_sections[stensor_iter].align_in_page);
        buf.size = ALIGN_PAGE(pbuf_templ.static_sections[stensor_iter].size);
        buf.real_size = pbuf_templ.static_sections[stensor_iter].size;
        pbuf.static_buf.push_back(buf);
        simulation_malloc_top = buf.pa + buf.size;
    }

    if (pbuf.static_buf.size())
    {
        pbuf.static_group.pa = pbuf.static_buf[0].pa;
        pbuf.static_group.va = pbuf.static_buf[0].va;
        pbuf.static_group.size = simulation_malloc_top - pbuf.static_buf[0].pa;
        pbuf.static_group.real_size = pbuf.static_group.size;
    }
    else
    {
        pbuf.static_group.pa = 0;
        pbuf.static_group.va = nullptr;
        pbuf.static_group.size = 0;
        pbuf.static_group.real_size = 0;
    }

    /* reuse data */
    for (uint32_t reuse_iter = 0; reuse_iter < tbuf_templ.reuse_sections.size(); reuse_iter++)
    {
        buf.pa = get_aligned_addr(simulation_malloc_top,
            tbuf_templ.reuse_sections[reuse_iter].align_in_page);
        buf.size = ALIGN_PAGE(tbuf_templ.reuse_sections[reuse_iter].size);
        buf.real_size = tbuf_templ.reuse_sections[reuse_iter].size;
        tbuf.reuse_buf.push_back(buf);
        simulation_malloc_top = buf.pa + buf.size;
    }
    tbuf.reuse_group.pa = tbuf.reuse_buf[0].pa;
    tbuf.reuse_group.va = tbuf.reuse_buf[0].va;
    tbuf.reuse_group.size = simulation_malloc_top - tbuf.reuse_buf[0].pa;
    tbuf.reuse_group.real_size = tbuf.reuse_group.size;

    /* alloc the whole data buffer */
    data_buf_alloc_size = simulation_malloc_top - data_phys_base;
    /* TBD */
    ret = malloc_buf(0, data_buf_alloc_size, 1, &giter->second.data);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }
    giter->second.data.pa = data_phys_base;

    /* alloc va for every internal data buffer */
    tbuf.stack.va = giter->second.data.va;
    for (uint32_t stensor_iter = 0; stensor_iter < pbuf_templ.static_sections.size(); stensor_iter++)
    {
        offset = pbuf.static_buf[stensor_iter].pa - data_phys_base;
        pbuf.static_buf[stensor_iter].va = (void*)((unsigned long)giter->second.data.va + offset);
    }
    for (uint32_t reuse_iter = 0; reuse_iter < tbuf_templ.reuse_sections.size(); reuse_iter++)
    {
        offset = tbuf.reuse_buf[reuse_iter].pa - data_phys_base;
        tbuf.reuse_buf[reuse_iter].va = (void*)((unsigned long)giter->second.data.va + offset);
    }

finish:
    return ret;
}
#endif

#if (defined X86_LINUX) && (X86_LINUX==1)
aipu_status_t AIRT::DeviceCtrl::simulation_free_data_buffer(uint32_t graph_id)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    std::map<uint32_t, simulation_res_t>::iterator giter = graphs.find(graph_id);

    if (graphs.end() == giter)
    {
        ret = AIPU_STATUS_ERROR_GRAPH_NOT_EXIST;
        goto finish;
    }

    if (nullptr == giter->second.data.va)
    {
        goto finish;
    }

    ret = free_buf(&giter->second.data);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }

    giter->second.data.va = nullptr;
    giter->second.data.pa = 0;
    giter->second.data.size = 0;

finish:
    return ret;
}
#endif

#if (defined X86_LINUX) && (X86_LINUX==1)
aipu_status_t AIRT::DeviceCtrl::simulation_set_io_info(uint32_t graph_id, const iobuf_info_t& iobuf)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    std::map<uint32_t, simulation_res_t>::iterator giter = graphs.find(graph_id);
    output_file_desc_t output;

    if (graphs.end() == giter)
    {
        ret = AIPU_STATUS_ERROR_GRAPH_NOT_EXIST;
        goto finish;
    }

    for (uint32_t i = 0; i < iobuf.outputs.number; i++)
    {
        output.id = iobuf.outputs.tensors[i].id;
        output.size = iobuf.outputs.tensors[i].size;
        output.va = iobuf.outputs.tensors[i].va;
        output.pa = iobuf.outputs.pa[i];
        snprintf(output.fname, sizeof(output.fname),
            "%s/Simulation_Graph0x%x_Output%u_Base0x%x_Size0x%x.bin",
            output_dir.c_str(), graph_id, output.id, output.pa, output.size);
        giter->second.outputs.push_back(output);
    }

    for (uint32_t i = 0; i < iobuf.pdata.number; i++)
    {
        output.id = iobuf.pdata.tensors[i].id;
        output.size = iobuf.pdata.tensors[i].size;
        output.va = iobuf.pdata.tensors[i].va;
        output.pa = iobuf.pdata.pa[i];
        snprintf(output.fname, sizeof(output.fname),
            "%s/Simulation_Graph0x%x_ProfilingData%u_Base0x%x_Size0x%x.bin",
            output_dir.c_str(), graph_id, output.id, output.pa, output.size);
        giter->second.outputs.push_back(output);
    }

    for (uint32_t i = 0; i < iobuf.plog_data.number; i++)
    {
        output.id = iobuf.plog_data.tensors[i].id;
        output.size = iobuf.plog_data.tensors[i].size;
        output.va = iobuf.plog_data.tensors[i].va;
        output.pa = iobuf.plog_data.pa[i];
        snprintf(output.fname, sizeof(output.fname),
            "%s/Simulation_Graph0x%x_PrintfData%u_Base0x%x_Size0x%x.bin",
            output_dir.c_str(), graph_id, output.id, output.pa, output.size);
        giter->second.outputs.push_back(output);
    }

finish:
    return ret;
}
#endif

aipu_status_t AIRT::DeviceCtrl::init()
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;

#if (defined ARM_LINUX) && (ARM_LINUX==1)
    int kern_ret = 0;
    aipu_open_info_t info;
    kern_ret = dev_op_wrapper_open(info);
    if (kern_ret != 0)
    {
        ret = AIPU_STATUS_ERROR_OPEN_DEV_FAIL;
        goto finish;
    }

    fd = info.fd;
    host_aipu_shm_offset = info.host_aipu_shm_offset;
    aipu_arch = AIPU_HW_ARCH_ZHOUYI;

    /* AIPU HW version */
    aipu_version = get_aipu_hw_version(info.cap.isa_version);

    /* AIPU HW config */
    aipu_hw_config = get_aipu_hw_config_version_number(info.cap.isa_version, info.cap.aiff_feature,
            info.cap.tpc_feature);
    LOG(LOG_DEBUG, "AIPU hardware version: %d, config version number: %d",
            aipu_version, aipu_hw_config);
#else
    has_additional_opt = false;
    simulation_malloc_top = 0;
    goto finish;
#endif

finish:
    return ret;
}

aipu_status_t AIRT::DeviceCtrl::deinit()
{
#if (defined ARM_LINUX) && (ARM_LINUX==1)
    if (fd > 0)
    {
        dev_op_wrapper_close(fd);
        fd = 0;
    }
    host_aipu_shm_offset = 0;
#else
    graphs.clear();
    simulation_cmd[0] = '\0';
    simulation_malloc_top = 0;
#endif
    return AIPU_STATUS_SUCCESS;
}

void AIRT::DeviceCtrl::unload_graph(uint32_t graph_id)
{
#if (defined X86_LINUX) && (X86_LINUX==1)
    graphs.erase(graph_id);
#endif
}

uint64_t AIRT::DeviceCtrl::get_shm_offset() const
{
    return host_aipu_shm_offset;
}

aipu_status_t AIRT::DeviceCtrl::malloc_buf(uint32_t dtype, uint32_t size, uint32_t align,
        buffer_desc_t* buf, uint32_t region_id)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
#if (defined ARM_LINUX) && (ARM_LINUX==1)
    int kern_ret = AIPU_ERRCODE_NO_ERROR;
#endif

    if (nullptr == buf)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    if (0 == size)
    {
        ret = AIPU_STATUS_ERROR_INVALID_SIZE;
        goto finish;
    }

#if (defined ARM_LINUX) && (ARM_LINUX==1)
    kern_ret = dev_op_wrapper_malloc(fd, dtype, size, align, buf, region_id);
    if (AIPU_ERRCODE_NO_ERROR != kern_ret)
    {
        ret = AIPU_STATUS_ERROR_BUF_ALLOC_FAIL;
    }
#else
    buf->pa = simulation_malloc_top;
    buf->va = new char[ALIGN_PAGE(size)];
    buf->size = ALIGN_PAGE(size);
    buf->real_size = size;
    simulation_malloc_top += buf->size;
#endif

#if (defined DEBUG_ZALLOC_ALL_FLAG) && (DEBUG_ZALLOC_ALL_FLAG==1)
    memset((void*)buf->va, 0, buf->size);
#endif

finish:
    return ret;
}

aipu_status_t AIRT::DeviceCtrl::free_buf(const buffer_desc_t* buf)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
#if (defined ARM_LINUX) && (ARM_LINUX==1)
    int kern_ret = AIPU_ERRCODE_NO_ERROR;
#endif

    if (nullptr == buf)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    if (0 == buf->size)
    {
        ret = AIPU_STATUS_ERROR_INVALID_SIZE;
        goto finish;
    }

#if (defined ARM_LINUX) && (ARM_LINUX==1)
    kern_ret = dev_op_wrapper_free(fd, buf);
    if (kern_ret != 0)
    {
        LOG(LOG_ERR, "free buffer ioctl failed! (errno = %d)", errno);
        ret = AIPU_STATUS_ERROR_BUF_FREE_FAIL;
        goto finish;
    }
#else
    if (nullptr != buf->va)
    {
        delete[] (char*)buf->va;
    }
#endif

    LOG(LOG_CLOSE, "buffer is freed: addr 0x%lx, size 0x%lx", buf->pa, buf->size);

finish:
    return ret;
}

aipu_status_t AIRT::DeviceCtrl::alloc_text_buffer(uint32_t graph_id, const pbuf_alloc_templ_t& pbuf_templ,
    buffer_desc_t& ibuf_desc)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;

    if (0 == pbuf_templ.text_size)
    {
        ret = AIPU_STATUS_ERROR_INVALID_SIZE;
        goto finish;
    }

#if (defined ARM_LINUX) && (ARM_LINUX==1)
    ret = malloc_buf(AIPU_MM_DATA_TYPE_TEXT, pbuf_templ.text_size, 1, &ibuf_desc);
#else
    if (graphs.count(graph_id) == 1)
    {
        ret = AIPU_STATUS_ERROR_INVALID_OP;
        goto finish;
    }
    /* reuse code section buffer in virtual space */
    ibuf_desc.va = (void*)pbuf_templ.text_src;
    ibuf_desc.pa = simulation_malloc_top;
    ibuf_desc.size = ALIGN_PAGE(pbuf_templ.text_size);
    ibuf_desc.real_size = pbuf_templ.text_size;
    simulation_malloc_top += ibuf_desc.size;
#endif

finish:
    return ret;
}

void AIRT::DeviceCtrl::load_buffer(volatile void* dest, const void* src, uint32_t bytes)
{
    for (uint32_t i = 0; i < bytes; i++)
    {
        *(volatile char*)((unsigned long)dest + i) = *(const char*)((unsigned long)src + i);
    }
}

aipu_status_t AIRT::DeviceCtrl::load_text_buffer(uint32_t graph_id, const void* src, uint32_t size,
    buffer_desc_t& ibuf_desc)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
#if (defined X86_LINUX) && (X86_LINUX==1)
    simulation_res_t sim;
#endif

    if ((nullptr == src) || (nullptr == ibuf_desc.va))
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    if ((0 == size) || (0 == ibuf_desc.size))
    {
        ret = AIPU_STATUS_ERROR_INVALID_SIZE;
        goto finish;
    }

#if (defined X86_LINUX) && (X86_LINUX==1)
    if (graphs.count(graph_id) == 1)
    {
        ret = AIPU_STATUS_ERROR_INVALID_OP;
        goto finish;
    }

    ret = create_simulation_input_file(graph_id, ibuf_desc, sim.code_fname, "Bin", size);
    if (AIPU_STATUS_SUCCESS != ret)
    {
        goto finish;
    }

    sim.graph_id = graph_id;
    graphs[graph_id] = sim;
#else
    load_buffer(ibuf_desc.va, src, size);
#endif

finish:
    return ret;
}

aipu_status_t AIRT::DeviceCtrl::free_text_buffer(buffer_desc_t& ibuf_desc)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
#if (defined ARM_LINUX) && (ARM_LINUX==1)
    ret = free_buf(&ibuf_desc);
#endif
    return ret;
}

aipu_status_t AIRT::DeviceCtrl::schedule_job_on_aipu(uint32_t graph_id, job_desc_t *job)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
    int kern_ret = 0;
#if (defined ARM_LINUX) && (ARM_LINUX==1)
    user_job job2kern;
#else
    std::map<uint32_t, simulation_res_t>::iterator giter = graphs.end();
#endif

    if(job == nullptr)
    {
        return AIPU_STATUS_ERROR_NULL_PTR;
    }

#if (defined ARM_LINUX) && (ARM_LINUX==1)
    if (fd <= 0)
    {
        ret = AIPU_STATUS_ERROR_OPEN_DEV_FAIL;
        goto finish;
    }

    job2kern.desc.job_id = job->id;
    job2kern.desc.start_pc_addr = job->config.code.start_pc_pa;
    job2kern.desc.intr_handler_addr = job->config.code.interrupt_pc_pa;
    job2kern.desc.data_0_addr = job->config.rodata_base;
    job2kern.desc.data_1_addr = job->config.stack_base;
    job2kern.desc.code_size = job->config.code_size;
    job2kern.desc.rodata_size = job->config.rodata_size;
    job2kern.desc.stack_size = job->config.stack_size;
    job2kern.desc.static_addr = job->config.static_base;
    job2kern.desc.static_size = job->config.static_size;
    job2kern.desc.reuse_addr = job->config.reuse_base;
    job2kern.desc.reuse_size = job->config.reuse_size;
    job2kern.desc.enable_prof = job->config.enable_prof;
    job2kern.desc.enable_asid = job->config.enable_asid;
    job2kern.errcode = AIPU_ERRCODE_NO_ERROR;
    kern_ret = ioctl(fd, IPUIOC_RUNJOB, &job2kern);
    if ((kern_ret != 0) || (job2kern.errcode != AIPU_ERRCODE_NO_ERROR))
    {
        LOG(LOG_ERR, "load aipu job descriptor to KMD failed! (errcode = %d)", job2kern.errcode);
        job->state = JOB_STATE_NO_STATE;
        goto finish;
    }
#endif

#if (defined X86_LINUX) && (X86_LINUX==1)
    pthread_mutex_lock(&glock);
    giter = graphs.find(graph_id);
    if (graphs.end() == giter)
    {
        ret = AIPU_STATUS_ERROR_GRAPH_NOT_EXIST;
        goto unlock;
    }

    ret = update_simulation_rtcfg(graph_id, job->config);
    if (ret != AIPU_STATUS_SUCCESS)
    {
        goto error;
    }

    LOG(LOG_DEFAULT, "[UMD SIMULATION] %s", simulation_cmd);
    kern_ret = system(simulation_cmd);
    if (kern_ret == -1)
    {
        LOG(LOG_ERR, "Simulation execution failed!");
        goto error;
    }
    else if (WIFEXITED(kern_ret) && (WEXITSTATUS(kern_ret) != 0))
    {
        LOG(LOG_ERR, "Simulation execution failed! (simulator ret = %d)", WEXITSTATUS(kern_ret));
        goto error;
    }
    else if (WIFSIGNALED(kern_ret))
    {
        LOG(LOG_ERR, "Simulation terminated by signal %d!", WTERMSIG(kern_ret));
        goto error;
    }

    if ((job->config.arch == AIPU_HW_ARCH_ZHOUYI) &&
        (job->config.hw_version == AIPU_HW_VERSION_ZHOUYI_V1) &&
        (use_new_arch_sim == 0))
    {
        if (0 != umd_load_file_helper(giter->second.odata_fname,
            giter->second.odata_whole_va, giter->second.odata_whole_size))
        {
            goto error;
        }
    }
    else
    {
        for (uint32_t i = 0; i < giter->second.outputs.size(); i++)
        {
            if (0 != umd_load_file_helper(giter->second.outputs[i].fname,
                giter->second.outputs[i].va, giter->second.outputs[i].size))
            {
                goto error;
            }
        }
    }

    job->state = JOB_STATE_DONE;
    ret = AIPU_STATUS_SUCCESS;
    LOG(LOG_INFO, "Simulation end.");
    goto unlock;

error:
    job->state = JOB_STATE_EXCEPTION;
    ret = AIPU_STATUS_ERROR_JOB_EXCEPTION;
    goto unlock;

unlock:
    pthread_mutex_unlock(&glock);
    goto finish;
#endif

finish:
    return ret;
}

aipu_status_t AIRT::DeviceCtrl::get_dev_status(uint32_t* value) const
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
#if (defined ARM_LINUX) && (ARM_LINUX==1)
    int kern_ret = 0;

    if (nullptr == value)
    {
        ret = AIPU_STATUS_ERROR_NULL_PTR;
        goto finish;
    }

    kern_ret = dev_op_wrapper_read32(fd, 0x4, value);
    if (kern_ret != 0)
    {
        /* TBD: errcode should be mapped */
        ret = AIPU_STATUS_ERROR_INVALID_CTX;
        goto finish;
    }

#else
    ret = AIPU_STATUS_ERROR_OP_NOT_SUPPORTED;
    goto finish;
#endif

finish:
    return ret;
}

bool AIRT::DeviceCtrl::match_target_dev(uint32_t arch, uint32_t version, uint32_t hw_config) const
{
#if (defined ARM_LINUX) && (ARM_LINUX==1)
    return (arch == aipu_arch) && (version == aipu_version) && (hw_config == aipu_hw_config);
#else
    return true;
#endif
}

aipu_status_t AIRT::DeviceCtrl::poll_status(std::vector<job_status_desc>& jobs_status, uint32_t max_cnt, uint32_t time_out,
    bool poll_single_job, uint32_t job_id)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;
#if (defined X86_LINUX) && (X86_LINUX==1)
    ret = AIPU_STATUS_ERROR_OP_NOT_SUPPORTED;
#else
    int kern_ret = 0;
    kern_ret = dev_op_wrapper_poll(fd, jobs_status, max_cnt, time_out, poll_single_job, job_id);
    if (kern_ret != 0)
    {
        /* TBD: mapping ret val */
        LOG(LOG_ERR, "poll failed: ret = %d", kern_ret);
        ret = AIPU_STATUS_ERROR_NULL_PTR; /* to be removed */
    }
#endif
    return ret;
}

aipu_status_t AIRT::DeviceCtrl::kill_timeout_job(uint32_t job_id)
{
    aipu_status_t ret = AIPU_STATUS_SUCCESS;

#if (defined ARM_LINUX) && (ARM_LINUX==1)
    if(ioctl(fd, IPUIOC_KILL_TIMEOUT_JOB, &job_id))
    {
        ret = AIPU_STATUS_ERROR_JOB_NOT_EXIST;
    }
#endif

    return ret;
}
