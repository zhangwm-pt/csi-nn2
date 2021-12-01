/*
 * Copyright (C) 2016-2021 C-SKY Limited. All rights reserved.
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

#include "test_utils.h"
#include "csi_nn.h"
#include "math_snr.h"

int main(int argc, char** argv)
{
    init_testsuite("Testing function of im2col nchw i8.\n");

    struct csi_tensor *input = csi_alloc_tensor(NULL);
    struct csi_tensor *output = csi_alloc_tensor(NULL);
    struct csi_tensor *reference = csi_alloc_tensor(NULL);
    struct im2col_params params;
    int in_size = 1, out_size = 1;
    int zp, quantized_multiplier, shift;
    float scale, min_value, max_value;
    float error[2] = {0.0f};
    float max_error = 0.0f;

    int *buffer = read_input_data_f32(argv[1]);

    input->dim[0]   = buffer[0];     //batch
    input->dim[1]   = buffer[1];     //in_channel
    input->dim[2]   = buffer[2];     //in_height
    input->dim[3]   = buffer[3];     //in_width
    input->dim_count = 4;

    params.kernel_h  = buffer[4];
    params.kernel_w  = buffer[5];
    params.stride_h  = buffer[6];
    params.stride_w  = buffer[7];
    params.pad_left  = buffer[8];
    params.pad_right = buffer[9];
    params.pad_top   = buffer[10];
    params.pad_down  = buffer[11];

    for(int i = 0; i < input->dim_count; i++) {
        in_size *= input->dim[i];
    }

    int out_h = (input->dim[2] + params.pad_top + params.pad_down - params.kernel_h) / params.stride_h + 1;
    int out_w = (input->dim[3] + params.pad_left + params.pad_right - params.kernel_w) / params.stride_w + 1;

    output->dim[0] = input->dim[1] * params.kernel_h * params.kernel_w;
    output->dim[1] = input->dim[0] * out_h * out_w;
    output->dim_count = 2;

    out_size = input->dim[0] * input->dim[1] * params.kernel_h * params.kernel_w * out_h * out_w;
    input->dtype = CSINN_DTYPE_INT8;
    output->dtype = CSINN_DTYPE_INT8;
    params.base.layout = CSINN_NCHW;
    params.base.api = CSINN_API;
    params.base.run_mode = CSINN_RM_LAYER;

    float *src_in   = (float *)(buffer + 12);
    float *ref      = (float *)(buffer + 12 + in_size);
    int8_t *src_tmp = (int8_t *)malloc(in_size * sizeof(int8_t));

    input->qinfo = get_quant_info_i8(src_in, in_size);

    for(int i = 0; i < in_size; i++) {
        src_tmp[i] = csi_ref_quantize_f32_to_i8(src_in[i], input->qinfo);
    }

    /* compute the input's max quantize error */
    for(int i = 0; i < in_size; i++) {
        float error1;
        float output_tmp  = csi_ref_dequantize_i8_to_f32(src_tmp[i], input->qinfo);
        if(isinf(src_in[i]) || isnan(src_in[i])){
            continue;
        } else {
            error1 = fabs(src_in[i] -output_tmp);
            if(error1 > 1e-6) {
                error1 = fabs(src_in[i] - output_tmp)/fabs(src_in[i] + 1e-9);
            }
        }
        if(error1 > error[0]) {
            error[0] = error1;
        }
    }


    output->qinfo = get_quant_info_i8(ref, out_size);

    int8_t *dst_tmp = (int8_t *)malloc(out_size * sizeof(int8_t));

    for(int i = 0; i < out_size; i++) {
        dst_tmp[i] = csi_ref_quantize_f32_to_i8(ref[i], output->qinfo);
    }

    /* compute the output's max quantize error */
    for(int i = 0; i < out_size; i++) {
        float error1;
        float output_tmp  = csi_ref_dequantize_i8_to_f32(dst_tmp[i], output->qinfo);
        if(isinf(ref[i]) || isnan(ref[i])){
            continue;
        } else {
            error1 = fabs(ref[i] -output_tmp);
            if(error1 > 1e-6) {
                error1 = fabs(ref[i] - output_tmp)/fabs(ref[i] + 1e-9);
            }
        }
        if(error1 > error[1]) {
            error[1] = error1;
        }
    }


    input->data     = src_tmp;
    reference->data = ref;
    output->data    = (int8_t *)malloc(out_size * sizeof(int8_t));

    max_error = (error[0] + error[1]);
    float difference = argc > 2 ? atof(argv[2]) :  max_error;

    if (csi_im2col_init(input, output, &params) == CSINN_TRUE) {
        csi_im2col(input, output, &params);
    }

    result_verify_8(reference->data, output, input->data, difference, out_size, false);

    free(buffer);
    free(src_tmp);
    free(output->data);
    return done_testing();
}