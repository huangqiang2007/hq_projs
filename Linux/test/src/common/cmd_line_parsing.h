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
 * @file  cmd_line_parsing.h
 * @brief AIPU UMD test header file: command line parsing
 */
#ifndef _CMD_LINE_PARSING_H_
#define _CMD_LINE_PARSING_H_

typedef struct cmd_opt {
    char simulator[255];
    char cfg_file_dir[255];
    char bin_file_name[255];
    char data_file_name[1000];
    char check_file_name[255];
    char dump_dir[255];
} cmd_opt_t;

void parsing_cmd_line(int argc, char* argv[], cmd_opt_t* opt, const char* test_case);

#endif /* _CMD_LINE_PARSING_H_ */