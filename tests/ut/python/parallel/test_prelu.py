# Copyright 2019 Huawei Technologies Co., Ltd
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
# http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import numpy as np

import mindspore as ms
import mindspore.nn as nn
from mindspore import Tensor
from mindspore import context
from mindspore.common.api import _cell_graph_executor
from mindspore.ops import composite as C
from mindspore.ops import operations as P
from tests.ut.python.ops.test_math_ops import VirtualLoss


grad_all = C.GradOperation(get_all=True)


class NetWithLoss(nn.Cell):
    def __init__(self, network):
        super(NetWithLoss, self).__init__()
        self.loss = VirtualLoss()
        self.network = network

    def construct(self, x, y):
        predict = self.network(x, y)
        return self.loss(predict)


class GradWrap(nn.Cell):
    def __init__(self, network):
        super(GradWrap, self).__init__()
        self.network = network

    def construct(self, x, y):
        return grad_all(self.network)(x, y)


def compile_net(net, x, y):
    net.set_auto_parallel()
    net.set_train()
    _cell_graph_executor.compile(net, x, y)


def test_prelu_single_success1():
    class Net(nn.Cell):
        def __init__(self):
            super().__init__()
            self.prelu = P.PReLU()

        def construct(self, x, y):
            out = self.prelu(x, y)
            return out

    context.reset_auto_parallel_context()
    net = GradWrap(NetWithLoss(Net()))
    x = Tensor(np.random.rand(1, 33, 4, 4), ms.float32)
    w = Tensor(np.random.rand(33), ms.float32)
    compile_net(net, x, w)


def test_prelu_single_success2():
    class Net(nn.Cell):
        def __init__(self):
            super().__init__()
            self.prelu = P.PReLU()

        def construct(self, x, y):
            out = self.prelu(x, y)
            return out

    context.reset_auto_parallel_context()
    net = GradWrap(NetWithLoss(Net()))
    x = Tensor(np.random.rand(1, 33, 4, 4), ms.float32)
    w = Tensor([0.1], ms.float32)
    compile_net(net, x, w)


def test_prelu_parallel_success1():
    class Net(nn.Cell):
        def __init__(self, strategy):
            super().__init__()
            self.prelu = P.PReLU().shard(strategy)

        def construct(self, x, y):
            out = self.prelu(x, y)
            return out

    context.reset_auto_parallel_context()
    context.set_auto_parallel_context(device_num=8, global_rank=0)
    context.set_auto_parallel_context(parallel_mode="semi_auto_parallel")
    strategy = ((1, 1, 1, 1), (1,))
    x = Tensor(np.random.rand(4, 4, 32, 64), dtype=ms.float32)
    w = Tensor(np.random.rand(4), dtype=ms.float32)
    net = GradWrap(NetWithLoss(Net(strategy)))
    compile_net(net, x, w)


def test_prelu_parallel_success2():
    class Net(nn.Cell):
        def __init__(self, strategy):
            super().__init__()
            self.prelu = P.PReLU().shard(strategy)

        def construct(self, x, y):
            out = self.prelu(x, y)
            return out

    context.reset_auto_parallel_context()
    context.set_auto_parallel_context(device_num=64, global_rank=0)
    context.set_auto_parallel_context(parallel_mode="semi_auto_parallel")
    strategy = ((2, 1, 4, 8), (1,))
    x = Tensor(np.random.rand(4, 4, 32, 64), dtype=ms.float32)
    w = Tensor(np.random.rand(4), dtype=ms.float32)
    net = GradWrap(NetWithLoss(Net(strategy)))
    compile_net(net, x, w)


def test_prelu_parallel_success3():
    class NetWithLoss3(nn.Cell):
        def __init__(self, network):
            super(NetWithLoss3, self).__init__()
            self.loss = VirtualLoss()
            self.network = network

        def construct(self, x, y, w):
            predict = self.network(x, y, w)
            return self.loss(predict)

    class GradWrap3(nn.Cell):
        def __init__(self, network):
            super(GradWrap3, self).__init__()
            self.network = network

        def construct(self, x, y, w):
            return grad_all(self.network)(x, y, w)

    class Net(nn.Cell):
        def __init__(self, strategy1, strategy2):
            super().__init__()
            self.matmul = P.MatMul().shard(strategy1)
            self.prelu = P.PReLU().shard(strategy2)

        def construct(self, x, y, w):
            out = self.matmul(x, y)
            out = self.prelu(out, w)
            return out

    context.reset_auto_parallel_context()
    context.set_auto_parallel_context(device_num=64, global_rank=0)
    context.set_auto_parallel_context(parallel_mode="semi_auto_parallel")
    strategy1 = ((2, 4), (4, 2))
    strategy2 = ((32, 1), (1,))
    x = Tensor(np.random.rand(128, 64), dtype=ms.float32)
    y = Tensor(np.random.rand(64, 16), dtype=ms.float32)
    w = Tensor(np.random.rand(16), dtype=ms.float32)
    net = GradWrap3(NetWithLoss3(Net(strategy1, strategy2)))
    net.set_auto_parallel()
    net.set_train()
    _cell_graph_executor.compile(net, x, y, w)


def test_prelu_parallel_success4():
    class Net(nn.Cell):
        def __init__(self, strategy):
            super().__init__()
            self.prelu = P.PReLU().shard(strategy)

        def construct(self, x, y):
            out = self.prelu(x, y)
            return out

    context.reset_auto_parallel_context()
    context.set_auto_parallel_context(device_num=64, global_rank=0)
    context.set_auto_parallel_context(parallel_mode="semi_auto_parallel")
    strategy = ((2, 4, 4, 2), (4,))
    x = Tensor(np.random.rand(4, 16, 32, 64), dtype=ms.float32)
    w = Tensor(np.random.rand(16), dtype=ms.float32)
    net = GradWrap(NetWithLoss(Net(strategy)))
    compile_net(net, x, w)


def test_prelu_parallel_success5():
    class Net(nn.Cell):
        def __init__(self, strategy):
            super().__init__()
            self.prelu = P.PReLU().shard(strategy)

        def construct(self, x, y):
            out = self.prelu(x, y)
            return out

    context.reset_auto_parallel_context()
    context.set_auto_parallel_context(device_num=64, global_rank=0)
    context.set_auto_parallel_context(parallel_mode="semi_auto_parallel")
    strategy = ((2, 4, 4, 2), (1,))
    x = Tensor(np.random.rand(4, 16, 32, 64), dtype=ms.float32)
    w = Tensor(np.random.rand(1), dtype=ms.float32)
    net = GradWrap(NetWithLoss(Net(strategy)))
    compile_net(net, x, w)
