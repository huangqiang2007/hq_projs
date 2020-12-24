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
 * @file  cmd_line_parsing.cpp
 * @brief AIPU UMD test implementation file: command line parsing
 */

#include <iostream>
#include <getopt.h>
#include <string.h>
#include "cmd_line_parsing.h"

static struct option opts[] = {
    { "help", no_argument, NULL, 'h' },
    { "sim", required_argument, NULL, 's' },
    { "cfg_dir", required_argument, NULL, 'C' },
    { "bin", required_argument, NULL, 'b' },
    { "idata", required_argument, NULL, 'i' },
    { "check", required_argument, NULL, 'c' },
    { "dump_dir", required_argument, NULL, 'd' },
    { NULL, 0, NULL, 0 }
};

static void show_help_msg(const char* test,
    bool sim, bool cfg_dir, bool bin, bool idata, bool check, bool dump_dir)
{
    std::cout << "AIPU UMD" << test << "test application" << std::endl;
    std::cout << "Options:" << std::endl;
    std::cout << "-h, --help\t\tshow the help message" << std::endl;
    if (sim)
    {
        std::cout <<
            "--sim=<simulator exe>\tprovide simulator full name"
            << std::endl;
    }
    if (cfg_dir)
    {
        std::cout <<
            "--cfg_dir=<dir>\tprovide a dir for UMD to create config file"
            << std::endl;
    }
    if (bin)
    {
        std::cout <<
            "--bin=<bin file>\tprovide the graph binary file"
            << std::endl;
    }
    if (idata)
    {
        std::cout <<
            "--idata=<data file>\tprovide the input data files (use <,> to separate multiple inputs)"
            << std::endl;
    }
    if (check)
    {
        std::cout <<
            "--check=<file>\t\tprovide output ground truth check file"
            << std::endl;
    }
    if (dump_dir)
    {
        std::cout <<
            "--dump_dir=<dir>\tprovide output dir for UMD to create intermediate & output files"
            << std::endl;
    }
}

void parsing_cmd_line(int argc, char* argv[], cmd_opt_t* opt, const char* test_case)
{
    extern char *optarg;
    int opt_idx = 0;
    int c = 0;

    if (nullptr == opt)
    {
        fprintf(stderr, "[TEST ERROR] invalid null pointer!\n");
        return;
    }

    while (1)
    {
        c = getopt_long (argc, argv, "hs:C:b:i:c:d:", opts, &opt_idx);
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

        case 'h':
            show_help_msg(test_case, 1, 1, 1, 1, 1, 1);
            exit(0);

        case 's':
            strcpy(opt->simulator, optarg);
            break;

        case 'C':
            strcpy(opt->cfg_file_dir, optarg);
            break;

        case 'b':
            strcpy(opt->bin_file_name, optarg);
            break;

        case 'i':
            strcpy(opt->data_file_name, optarg);
            break;

        case 'c':
            strcpy(opt->check_file_name, optarg);
            break;

        case 'd':
            strcpy(opt->dump_dir, optarg);
            break;

        case '?':
            break;

        default:
            break;
        }
    }

}