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

#include "csi_ref.h"

int csi_ref_floor_divide_f32(struct csi_tensor *input0,
                             struct csi_tensor *input1,
                             struct csi_tensor *output,
                             struct diso_params *params)
{
    float *input0_data = input0->data;
    float *input1_data = input1->data;
    float *output_data = output->data;
    int size = csi_tensor_size(input0);

    for (int i = 0; i < size; i++) {
        output_data[i] = floor(input0_data[i] / input1_data[i]);
    }
    return CSINN_TRUE;
}

int csi_ref_floor_divide_quant(struct csi_tensor *input0,
                               struct csi_tensor *input1,
                               struct csi_tensor *output,
                               struct diso_params *params)
{
    return csi_ref_diso_callback_base(input0, input1, output, params, csi_ref_floor_divide_f32);
}
