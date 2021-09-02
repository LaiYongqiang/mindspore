/**
 * Copyright 2021 Huawei Technologies Co., Ltd
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/**
 *Note:
 *  LJForceWithVirialEnergyUpdate. This is an experimental interface that is subject to change and/or deletion.
 */
#include "backend/kernel_compiler/gpu/sponge/lj/lj_force_with_virial_energy_update_kernel.h"

namespace mindspore {
namespace kernel {
MS_REG_GPU_KERNEL_THREE(LJForceWithVirialEnergyUpdate,
                        KernelAttr()
                          .AddInputAttr(kNumberTypeUInt32)
                          .AddInputAttr(kNumberTypeInt32)
                          .AddInputAttr(kNumberTypeFloat32)
                          .AddInputAttr(kNumberTypeFloat32)
                          .AddInputAttr(kNumberTypeInt32)
                          .AddInputAttr(kNumberTypeInt32)
                          .AddInputAttr(kNumberTypeFloat32)
                          .AddInputAttr(kNumberTypeFloat32)
                          .AddInputAttr(kNumberTypeFloat32)
                          .AddOutputAttr(kNumberTypeFloat32)
                          .AddOutputAttr(kNumberTypeFloat32)
                          .AddOutputAttr(kNumberTypeFloat32),
                        LJForceWithVirialEnergyUpdateGpuKernel, float, int, unsigned int)
}  // namespace kernel
}  // namespace mindspore