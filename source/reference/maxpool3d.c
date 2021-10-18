/*
 * Copyright (C) 2016-2020 C-SKY Limited. All rights reserved.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the License); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an AS IS BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "csi_nn.h"
#include "csi_utils.h"

static int csi_maxpool3d_ncdhw_f32(struct csi_tensor *input,
                                struct csi_tensor *output,
                                struct pool_params *params)
{
    float *input_data  = (float *)input->data;
    float *output_data = (float *)output->data;

    const int batch = input->dim[0];
    const int channel = input->dim[1];
    const int in_depth = input->dim[2];
    const int in_height = input->dim[3];
    const int in_width = input->dim[4];
    const int out_depth = output->dim[2];
    const int out_height = output->dim[3];
    const int out_width = output->dim[4];

    for(int in_ch=0; in_ch<batch; ++in_ch) {
        for(int out_ch=0; out_ch<channel; ++out_ch) {
            for(int out_d=0; out_d<out_depth; ++out_d) {
                for(int out_h=0; out_h<out_height; ++out_h) {
                    for(int out_w=0; out_w<out_width; ++out_w) {

                        const int in_d_origin = (out_d * params->stride_depth) - params->pad_front;
                        const int in_h_origin = (out_h * params->stride_height) - params->pad_top;
                        const int in_w_origin = (out_w * params->stride_width) - params->pad_left;
                        // Compute the boundaries of the filter region clamped so as to
                        // ensure that the filter window fits in the input array.
                        const int filter_d_begin = csi_max_internal_s32(0, -in_d_origin);
                        const int filter_d_end = csi_min_internal_s32(params->filter_depth, in_depth - in_d_origin);
                        const int filter_h_begin = csi_max_internal_s32(0, -in_h_origin);
                        const int filter_h_end = csi_min_internal_s32(params->filter_height, in_height - in_h_origin);
                        const int filter_w_begin = csi_max_internal_s32(0, -in_w_origin);
                        const int filter_w_end = csi_min_internal_s32(params->filter_width, in_width - in_w_origin);

                        float max = -1000;
                        int filter_cnt = 0;
                        for(int filter_d=filter_d_begin; filter_d<filter_d_end; ++filter_d) {
                            for(int filter_h=filter_h_begin; filter_h<filter_h_end; ++filter_h) {
                                for(int filter_w=filter_w_begin; filter_w<filter_w_end; ++filter_w) {
                                    int in_d = in_d_origin + filter_d;
                                    int in_h = in_h_origin + filter_h;
                                    int in_w = in_w_origin + filter_w;
                                    max = fmax(max, input_data[csi_get_index_5(input->dim, in_ch, out_ch, in_d, in_h, in_w)]);
                                    filter_cnt++;
                                }
                            }
                        }
                        if(filter_cnt != params->filter_depth * params->filter_height * params->filter_width) {
                            max = fmax(max, 0);
                        }
                        output_data[csi_get_index_5(output->dim, in_ch, out_ch, out_d, out_h, out_w)] = max;
                    }
                }
            }
        }
    }
    return CSINN_TRUE;
}

static int csi_maxpool3d_ncdhw_u8(struct csi_tensor *input,
                                struct csi_tensor *output,
                                struct pool_params *params)
{
    uint8_t *input_data  = (uint8_t *)input->data;
    uint8_t *output_data = (uint8_t *)output->data;

    const int batch = input->dim[0];
    const int channel = input->dim[1];
    const int in_depth = input->dim[2];
    const int in_height = input->dim[3];
    const int in_width = input->dim[4];
    const int out_depth = output->dim[2];
    const int out_height = output->dim[3];
    const int out_width = output->dim[4];

    const int32_t input_offset = input->offset;
    const int32_t input_multiplier = input->multiplier;
    const int32_t input_shift = input->shift;
    const int32_t output_offset = output->offset;
    const int32_t output_multiplier = output->multiplier;
    const int32_t output_shift = output->shift;

    for(int in_ch=0; in_ch<batch; ++in_ch) {
        for(int out_ch=0; out_ch<channel; ++out_ch) {
            for(int out_d=0; out_d<out_depth; ++out_d) {
                for(int out_h=0; out_h<out_height; ++out_h) {
                    for(int out_w=0; out_w<out_width; ++out_w) {

                        const int in_d_origin = (out_d * params->stride_depth) - params->pad_front;
                        const int in_h_origin = (out_h * params->stride_height) - params->pad_top;
                        const int in_w_origin = (out_w * params->stride_width) - params->pad_left;
                        // Compute the boundaries of the filter region clamped so as to
                        // ensure that the filter window fits in the input array.
                        const int filter_d_begin = csi_max_internal_s32(0, -in_d_origin);
                        const int filter_d_end = csi_min_internal_s32(params->filter_depth, in_depth - in_d_origin);
                        const int filter_h_begin = csi_max_internal_s32(0, -in_h_origin);
                        const int filter_h_end = csi_min_internal_s32(params->filter_height, in_height - in_h_origin);
                        const int filter_w_begin = csi_max_internal_s32(0, -in_w_origin);
                        const int filter_w_end = csi_min_internal_s32(params->filter_width, in_width - in_w_origin);

                        uint8_t max = 0;
                        int32_t filter_cnt = 0;
                        for(int filter_d=filter_d_begin; filter_d<filter_d_end; ++filter_d) {
                            for(int filter_h=filter_h_begin; filter_h<filter_h_end; ++filter_h) {
                                for(int filter_w=filter_w_begin; filter_w<filter_w_end; ++filter_w) {
                                    int in_d = in_d_origin + filter_d;
                                    int in_h = in_h_origin + filter_h;
                                    int in_w = in_w_origin + filter_w;
                                    max = csi_max_internal_s32(max, input_data[csi_get_index_5(input->dim, in_ch, out_ch, in_d, in_h, in_w)]);
                                    filter_cnt++;
                                }
                            }
                        }
                        if(filter_cnt != params->filter_depth * params->filter_height * params->filter_width) {
                            max = csi_max_internal_s32(max , input_offset);
                        }
                        // max = (filter_count == 0) ? -input_offset : max;
                        max = csi_requantize_u8(max, input_offset, input_multiplier, input_shift, output_offset, output_multiplier, output_shift);
                        output_data[csi_get_index_5(output->dim, in_ch, out_ch, out_d, out_h, out_w)] = max;
                    }
                }
            }
        }
    }
    return CSINN_TRUE;
}


static int csi_maxpool3d_ndhwc_f32(struct csi_tensor *input,
                                struct csi_tensor *output,
                                struct pool_params *params)
{
    float *input_data  = (float *)input->data;
    float *output_data = (float *)output->data;

    return CSINN_FALSE;
}

static int csi_maxpool3d_ndhwc_u8(struct csi_tensor *input,
                                struct csi_tensor *output,
                                struct pool_params *params)
{
    uint8_t *input_data  = (uint8_t *)input->data;
    uint8_t *output_data = (uint8_t *)output->data;

    return CSINN_FALSE;
}

int csi_maxpool3d_init(struct csi_tensor *input,
                    struct csi_tensor *output,
                    struct pool_params *params)
{
    if(params->layout == CSINN_NCDHW) {
        if(input->dtype == CSINN_DTYPE_UINT8) {
            params->bc = csi_maxpool3d_ncdhw_u8;
        } else if(input->dtype == CSINN_DTYPE_FLOAT32) {
            params->bc = csi_maxpool3d_ncdhw_f32;
        } else {
            return CSINN_UNSUPPORT_DTYPE;
        }
    } else if(params->layout == CSINN_NDHWC) {
        if(input->dtype == CSINN_DTYPE_UINT8) {
            params->bc = csi_maxpool3d_ndhwc_u8;
        } else if(input->dtype == CSINN_DTYPE_FLOAT32) {
            params->bc = csi_maxpool3d_ndhwc_f32;
        } else {
            return CSINN_UNSUPPORT_DTYPE;
        }
    } else {
        return CSINN_UNSUPPORT_LAYOUT;
    }
    return CSINN_TRUE;
}

int csi_maxpool3d(struct csi_tensor *input,
                struct csi_tensor *output,
                struct pool_params *params)
{
    if(params->bc !=NULL) {
        params->bc(input, output, params);
    } else {
        return CSINN_CALLBACK_UNSET;
    }
    return CSINN_TRUE;
}