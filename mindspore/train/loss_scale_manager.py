# Copyright 2020 Huawei Technologies Co., Ltd
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
# ============================================================================
"""Loss scale manager abstract class."""

from .._checkparam import Validator as validator
from .. import nn


class LossScaleManager:
    """
    Loss scale manager abstract class.

    Derive FixedLossScaleManager and DynamicLossScaleManager that override all LossScaleManager's method.
    """
    def get_loss_scale(self):
        """Get loss scale value."""

    def update_loss_scale(self, overflow):
        """
        Update loss scale value.

        Args:
            overflow (bool): Whether it overflows.
        """
    def get_update_cell(self):
        """Get the loss scaling update logic cell."""


class FixedLossScaleManager(LossScaleManager):
    """
    Loss scale with a fixed value, inherits from LossScaleManager.

    Args:
        loss_scale (float): Loss scale. Note that if `drop_overflow_update` is set to False, the value of `loss_scale`
            in optimizer that you used need to be set to the same value as here. Default: 128.0.
        drop_overflow_update (bool): Whether to execute optimizer if there is an overflow. If True, the optimizer will
            not executed when overflow occurs. Default: True.

    Examples:
        >>> from mindspore import Model, nn, FixedLossScaleManager
        >>>
        >>> net = Net()
        >>> #1) Drop the parameter update if there is an overflow
        >>> loss_scale_manager = FixedLossScaleManager()
        >>> optim = nn.Momentum(params=net.trainable_params(), learning_rate=0.1, momentum=0.9)
        >>> model = Model(net, loss_scale_manager=loss_scale_manager, optimizer=optim)
        >>>
        >>> #2) Execute parameter update even if overflow occurs
        >>> loss_scale = 1024.0
        >>> loss_scale_manager = FixedLossScaleManager(loss_scale, False)
        >>> optim = nn.Momentum(params=net.trainable_params(), learning_rate=0.1, momentum=0.9, loss_scale=loss_scale)
        >>> model = Model(net, loss_scale_manager=loss_scale_manager, optimizer=optim)
    """
    def __init__(self, loss_scale=128.0, drop_overflow_update=True):
        if loss_scale < 1:
            raise ValueError("The argument 'loss_scale' must be >= 1, "
                             "but got {}".format(loss_scale))
        self._loss_scale = loss_scale
        self._drop_overflow_update = drop_overflow_update

    def get_loss_scale(self):
        """
        Get loss scale value.

        Returns:
            bool, `loss_scale` value.
        """
        return self._loss_scale

    def get_drop_overflow_update(self):
        """
        Get the flag whether to drop optimizer update when there is an overflow.

        Returns:
            bool, `drop_overflow_update` value.
        """
        return self._drop_overflow_update

    def update_loss_scale(self, overflow):
        """
        Update loss scale value. The interface at `FixedLossScaleManager` will do nothing.

        Args:
            overflow (bool): Whether it overflows.
        """

    def get_update_cell(self):
        """
        Returns the update cell for `TrainOneStepWithLossScaleCell`.

        Returns:
            None or Cell. Cell object, used to update `loss_scale`, when `drop_overflow_update` is True. None when
            `drop_overflow_update` is False.
        """
        if not self._drop_overflow_update:
            return None
        return nn.FixedLossScaleUpdateCell(self._loss_scale)


class DynamicLossScaleManager(LossScaleManager):
    """
    Loss scale that dynamically adjusts itself, inherits from LossScaleManager.

    Args:
        init_loss_scale (float): Initialize loss scale. Default: 2**24.
        scale_factor (int): Coefficient of increase and decrease. Default: 2.
        scale_window (int): Maximum continuous normal steps when there is no overflow. Default: 2000.

    Examples:
        >>> from mindspore import Model, nn, DynamicLossScaleManager
        >>>
        >>> net = Net()
        >>> loss_scale_manager = DynamicLossScaleManager()
        >>> optim = nn.Momentum(params=net.trainable_params(), learning_rate=0.1, momentum=0.9)
        >>> model = Model(net, loss_scale_manager=loss_scale_manager, optimizer=optim)
    """
    def __init__(self,
                 init_loss_scale=2 ** 24,
                 scale_factor=2,
                 scale_window=2000):
        if init_loss_scale < 1.0:
            raise ValueError("The argument 'init_loss_scale' must be > 1, but got {}".format(init_loss_scale))
        self.loss_scale = init_loss_scale
        validator.check_positive_int(scale_window, "scale_window", self.__class__.__name__)
        self.scale_window = scale_window
        if scale_factor <= 0:
            raise ValueError("The argument 'scale_factor' must be > 0, but got {}".format(scale_factor))
        self.scale_factor = scale_factor
        self.increase_ratio = scale_factor
        self.decrease_ratio = 1 / scale_factor
        self.cur_iter = 1
        self.last_overflow_iter = 0
        self.bad_step_max = 1000
        self.bad_step = 0

    def get_loss_scale(self):
        """
        Get loss scale value.

        Returns:
            bool, `loss_scale` value.
        """
        return self.loss_scale

    def update_loss_scale(self, overflow):
        """
        Update loss scale value.

        Args:
            overflow (bool): Whether it overflows.
        """
        if overflow:
            self.loss_scale = max(self.loss_scale * self.decrease_ratio, 1)
            self.last_overflow_iter = self.cur_iter
            self.bad_step += 1
        else:
            if (self.cur_iter - self.last_overflow_iter) % self.scale_window == 0:
                self.loss_scale *= self.increase_ratio
            self.bad_step = 0

        if self.bad_step > self.bad_step_max:
            raise RuntimeError("Dynamic loss scale Continuous overflow ", self.bad_step,
                               " times, has exceeded maximum threshold.")

        self.cur_iter += 1

    def get_drop_overflow_update(self):
        """
        Get the flag whether to drop optimizer update when there is an overflow.

        Returns:
            bool, always return True at `DynamicLossScaleManager`.
        """
        return True

    def get_update_cell(self):
        """
        Returns the update cell for `TrainOneStepWithLossScaleCell`.

        Returns:
            Cell, cell object used to update `loss_scale`.
        """
        return nn.DynamicLossScaleUpdateCell(self.loss_scale, self.scale_factor, self.scale_window)
