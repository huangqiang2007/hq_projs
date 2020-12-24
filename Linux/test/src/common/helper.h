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
#ifndef _HELPER_H_
#define _HELPER_H_

#include <vector>
#include "standard_api.h"
#include "graph_test_info.h"

int dump_file_helper(const char* fname, void* src, unsigned int size);
int load_file_helper(const char* fname, char** dest);
bool is_output_correct(volatile char* src1, char* src2, uint32_t cnt);

#endif /* _HELPER_H_ */