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
    init_testsuite("Testing function of segment sum u8.\n");

    struct csi_tensor *input = csi_alloc_tensor(NULL);
    struct csi_tensor *output = csi_alloc_tensor(NULL);
    struct csi_tensor *reference = csi_alloc_tensor(NULL);
    struct csi_tensor *segment = csi_alloc_tensor(NULL);
    struct segment_params params;
    int in_size, out_size, zp, quantized_multiplier, shift;
    float max_value, min_value, scale;
    float error = 0;

    int *buffer = read_input_data_f32(argv[1]);
    input->dim[0] = buffer[0];          // batch
    input->dim[1] = buffer[1];          // height
    input->dim[2] = buffer[2];          // width
    input->dim[3] = buffer[3];          // channel
    output->dim[0] = buffer[4];          // batch
    output->dim[1] = buffer[1];          // height
    output->dim[2] = buffer[2];          // width
    output->dim[3] = buffer[3];          // channel

    input->dim_count = 4;
    output->dim_count = 4;
    input->dtype = CSINN_DTYPE_UINT8;
    output->dtype = CSINN_DTYPE_UINT8;
    params.num_segments = buffer[4];
    params.unsorted = CSINN_FALSE;
    params.base.api = CSINN_API;
    params.base.run_mode = CSINN_RM_LAYER;

    in_size  = input->dim[0] * input->dim[1] * input->dim[2] * input->dim[3];
    out_size = output->dim[0] * output->dim[1] * output->dim[2] * output->dim[3];

    uint8_t *input_tmp = malloc(in_size * sizeof(char));
    float   *src_in    = (float *)(buffer + 5);
    float   *ref       = (float *)(buffer + 5 + in_size + buffer[0]);;

    input->qinfo = get_quant_info(src_in, in_size);
    for(int i = 0; i < in_size; i++) {
        input_tmp[i] = csi_ref_quantize_f32_to_u8(src_in[i], input->qinfo);
    }
    /* compute the max quantize error */
    for(int i = 0; i < in_size; i++) {
        float error1;
        float output_tmp  = csi_ref_dequantize_u8_to_f32(src_in[i], input->qinfo);
        if(src_in[i] == INFINITY && output_tmp == INFINITY || src_in[i] == NAN && output_tmp == NAN){
            continue;
        } else {
            error1 = fabs(src_in[i] -output_tmp);
            if(error1 > 1e-6) {
                error1 = fabs(src_in[i] - output_tmp)/fabs(src_in[i] + 1e-9);
            }
        }
        if(error1 > error) {
            error = error1;
        }
    }
    /* sum */
    error = error * (input->dim[0] - params.num_segments + 1);

    output->qinfo = get_quant_info(ref, out_size);

    input->data      = input_tmp;
    reference->data  = ref;
    segment->data    = (int *)(buffer + 5 + in_size);
    output->data     = malloc(out_size * sizeof(char));


    float difference = argc > 2 ? *argv[2] : error;
    printf("The max error is %.6lf.\n", error);

    if (csi_segment_sum_init(input, segment, output, &params) == CSINN_TRUE) {
        csi_segment_sum(input, segment, output, &params);
    }

    result_verify_8(reference->data, output, input->data, difference, out_size, false);

    free(buffer);
    free(input_tmp);
    free(output->data);
    return done_testing();
}