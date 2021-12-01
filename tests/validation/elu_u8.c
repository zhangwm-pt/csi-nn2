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

/* CSI-NN2 version 1.8.x */

#include "test_utils.h"
#include "csi_nn.h"
#include "math_snr.h"

int main(int argc, char** argv)
{
    init_testsuite("Testing function of elu u8.\n");

    struct csi_tensor *input = csi_alloc_tensor(NULL);
    struct csi_tensor *output = csi_alloc_tensor(NULL);
    struct csi_tensor *reference = csi_alloc_tensor(NULL);
    struct relu_params params;
    int in_size = 0;
    int out_size =0;
    float max_error = 0.0f;

    int *buffer = read_input_data_f32(argv[1]);
    input->dim[0] = buffer[0];          // batch
    input->dim[1] = buffer[1];          // channel
    input->dim[2] = buffer[2];          // height
    input->dim[3] = buffer[3];          // width

    output->dim[0] = input->dim[0];
    output->dim[0] = input->dim[1];
    output->dim[0] = input->dim[2];
    output->dim[0] = input->dim[3];

    input->dim_count = 4;
    output->dim_count = 4;
    input->dtype = CSINN_DTYPE_UINT8;
    output->dtype = CSINN_DTYPE_UINT8;
    in_size = input->dim[0] * input->dim[1] * input->dim[2] * input->dim[3];
    out_size = in_size;
    params.base.api = CSINN_API;
    params.base.run_mode = CSINN_RM_LAYER;

    float *src_in   = (float *)(buffer + 4);
    float *ref      = (float *)(buffer + 4 + in_size);
    uint8_t *src_tmp = malloc(in_size * sizeof(char));

    input->data = src_in;
    get_quant_info(input);

    for(int i = 0; i < in_size; i++) {
        src_tmp[i] = csi_ref_quantize_f32_to_u8(src_in[i], input->qinfo);
    }

    output->data = ref;
    get_quant_info(output);

    input->data     = src_tmp;
    reference->data = ref;
    output->data    = malloc(in_size * sizeof(char));

    float difference = argc > 2 ? atof(argv[2]) : max_error;


    if (csi_elu_init(input, output, &params) == CSINN_TRUE) {
        csi_elu(input, output, &params);
    }

    result_verify_8(reference->data, output, input->data, difference, in_size, false);

    free(buffer);
    free(src_tmp);
    free(output->data);
    return done_testing();
}
