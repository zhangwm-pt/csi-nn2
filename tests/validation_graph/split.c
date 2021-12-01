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
    init_testsuite("Testing function of split(graph).\n");

    int *buffer = read_input_data_f32(argv[1]);
    int axis = buffer[4];
    int output_cnt = buffer[5];
    int32_t *split_index = (int32_t *)malloc(output_cnt * sizeof(int32_t));
    for(int i = 0; i < output_cnt; i++) {
        split_index[i] = buffer[axis] / output_cnt;
    }

    struct csi_tensor *reference[output_cnt];
    for(int i = 0; i < output_cnt; i++) {
        reference[i] = csi_alloc_tensor(NULL);
    }
    float min_value, max_value;
    int in_size = 0;
    int out_size[output_cnt];
    int acc_out_size = 0;   // in fact, different output tensor may has different out_size

    /* session configuration */
    struct csi_session *sess = csi_alloc_session();
    sess->base_api = CSINN_API;
    csi_session_init(sess);
    csi_set_input_number(1, sess);
    csi_set_output_number(output_cnt, sess);


    /* input tensor configuration */
    struct csi_tensor *input = csi_alloc_tensor(sess);
    input->dim[0] = buffer[0];          // batch
    input->dim[1] = buffer[1];          // channel
    input->dim[2] = buffer[2];          // height
    input->dim[3] = buffer[3];          // width
    input->dim_count = 4;
    in_size = input->dim[0] * input->dim[1] * input->dim[2] * input->dim[3];
    input->name = "input";
    float *input_data = (float *)(buffer + 6);
    input->data = input_data;
    get_quant_info(input);

    void *src_tmp = malloc(in_size * sizeof(char));
    for(int i = 0; i < in_size; i++) {
        if (sess->base_dtype == CSINN_DTYPE_UINT8) {
            *((uint8_t *)src_tmp + i) = csi_ref_quantize_f32_to_u8(input_data[i], input->qinfo);
        } else if (sess->base_dtype == CSINN_DTYPE_INT8) {
            *((int8_t *)src_tmp + i) = csi_ref_quantize_f32_to_i8(input_data[i], input->qinfo);
        }
    }


    /* output tensor configuration */
    struct csi_tensor *output[output_cnt];
    char output_name[output_cnt][10];
    for(int i = 0; i < output_cnt; i++) {
        output[i]  = csi_alloc_tensor(sess);
        for(int j = 0; j < 4; j++) {
            if(j == axis) {
                output[i]->dim[j] = split_index[i];
            } else {
                output[i]->dim[j] = input->dim[j];
            }
        }
        output[i]->dim_count = 4;
        out_size[i] = output[i]->dim[0] * output[i]->dim[1] * output[i]->dim[2] * output[i]->dim[3];
        reference[i]->data = (float *)(buffer + 6 + in_size + acc_out_size);
        acc_out_size += out_size[i];
        output[i]->data = reference[i]->data;
        get_quant_info(output[i]);
        sprintf(output_name[i], "output_%d", i);
        output[i]->name = output_name[i];
        output[i]->is_const = 0;
    }


    /* operator parameter configuration */
    struct split_params params;
    params.base.api = CSINN_API;
    params.base.name = "params";
    params.base.layout = CSINN_LAYOUT_NCHW;
    params.base.run_mode = CSINN_RM_NPU_GRAPH;
    params.axis = axis;
    params.output_num = output_cnt;
    int temp = 0;
    for(int i = 0; i < output_cnt; i++) {
        temp += split_index[i];
        split_index[i] = temp;
        printf("%d\n", split_index[i]);
    }
    params.split_index = split_index;


    /*
    light:
        split_index need Accumulate, such as split input shape[5, 30] to [5, 4] [5, 8] [5,18]   -->  split_index = [4, 12]
    */
    if (csi_split_init(input, (struct csi_tensor **)&output, &params) != CSINN_TRUE) {
        printf("split init fail.\n\t");
        return -1;
    }

    csi_set_tensor_entry(input, sess);
    csi_set_input(0, input, sess);

    csi_split(input, (struct csi_tensor **)&output, &params);

    for(int i = 0; i < output_cnt; i++) {
        csi_set_output(i, output[i], sess);
    }
    csi_session_setup(sess);

    struct csi_tensor *input_tensor = csi_alloc_tensor(NULL);
    if (sess->base_dtype == CSINN_DTYPE_FLOAT32) {
        input_tensor->data = input_data;
    } else if (sess->base_dtype == CSINN_DTYPE_UINT8 || sess->base_dtype == CSINN_DTYPE_INT8) {
        input_tensor->data = src_tmp;
    }
    csi_update_input(0, input_tensor, sess);
    csi_session_run(sess);


    struct csi_tensor *output_tensor[output_cnt];
    for(int i = 0; i < output_cnt; i++) {
        output_tensor[i] = csi_alloc_tensor(NULL);
        output_tensor[i]->data = NULL;
        output_tensor[i]->dtype = sess->base_dtype;
        output_tensor[i]->is_const = 0;
    }
    int output_num = csi_get_output_number(sess);
    printf("output_num = %d\n", output_num);
    for(int i = 0; i < output_num; i++) {   // output_cnt = output_num
        csi_get_output(i, output_tensor[i], sess);
        memcpy(output_tensor[i]->qinfo, output[i]->qinfo, sizeof(struct csi_quant_info));
    }


    /* verify result */
    float difference = argc > 2 ? atof(argv[2]) : 1e-4;
    for(int i = 0; i < output_cnt; i++) {
        if (sess->base_dtype == CSINN_DTYPE_UINT8 || sess->base_dtype == CSINN_DTYPE_INT8) {
            result_verify_8(reference[i]->data, output_tensor[i], input->data, difference, out_size[i], false);
        } else if (sess->base_dtype == CSINN_DTYPE_FLOAT32) {
            result_verify_f32(reference[i]->data, output_tensor[i]->data, input->data, difference, out_size[i], false);
        }

    }

    /* free alloced memory */
    free(buffer);
    free(input_tensor->qinfo);
    free(input_tensor);
    for(int i = 0; i < output_cnt; i++) {
        free(output_tensor[i]->qinfo);
        free(output_tensor[i]);
        free(reference[i]->qinfo);
        free(reference[i]);
    }
    free(src_tmp);
    free(split_index);

    csi_session_deinit(sess);
    csi_free_session(sess);
    return done_testing();
}
