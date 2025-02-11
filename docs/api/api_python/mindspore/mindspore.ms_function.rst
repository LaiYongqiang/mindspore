mindspore.ms_function
=====================

.. py:class:: mindspore.ms_function(fn=None, obj=None, input_signature=None)

    将Python函数编译为一张可调用的MindSpore图。

    MindSpore可以在运行时对图进行优化。

    **参数：**

        - **fn**  (Function)：要编译成图的Python函数。默认值：None。
        - **obj**  (Object)：用于区分编译后函数的Python对象。默认值：None。
        - **input_signature**  (Tensor)：用于表示输入参数的Tensor。Tensor的shape和dtype将作为函数的输入shape和dtype。如果指定了 `input_signature` ，则 `fn` 的每个输入都必须是 `Tensor` 。
            
    并且 `fn` 的输入参数将不会接受 `\**kwargs` 参数。实际输入的shape和dtype必须与 `input_signature` 的相同。否则，将引发TypeError。默认值：None。

    **返回：**

        函数，如果 `fn` 不是None，则返回一个已经将输入 `fn` 编译成图的可执行函数；如果 `fn` 为None，则返回一个装饰器。当这个装饰器使用单个 `fn` 参数进行调用时，等价于 `fn` 不是None的场景。

    **支持平台：**

        ``Ascend`` ``GPU`` ``CPU``

    **样例：**

    .. code-block::

        >>> import numpy as np
        >>> from mindspore import Tensor
        >>> from mindspore import ms_function
        ...
        >>> x =tensor(np.ones([1,1,3,3]).astype(np.float32))
        >>> y =tensor(np.ones([1,1,3,3]).astype(np.float32))
        ...
        >>> # 通过调用ms_function创建可调用的MindSpore图
        >>> def tensor_add(x, y):
        ...     z = x + y
        ...     return z
        ...
        >>> tensor_add_graph = ms_function(fn=tensor_add)
        >>> out = tensor_add_graph(x, y)
        ...
        >>> # 通过装饰器@ms_function创建一个可调用的MindSpore图
        >>> @ms_function
        ... def tensor_add_with_dec(x, y):
        ...     z = x + y
        ...     return z
        ...
        >>> out = tensor_add_with_dec(x, y)
        ...
        >>> # 通过带有input_signature参数的装饰器@ms_function创建一个可调用的MindSpore图
        >>> @ms_function(input_signature=(Tensor(np.ones([1, 1, 3, 3]).astype(np.float32)),
        ...                               Tensor(np.ones([1, 1, 3, 3]).astype(np.float32))))
        ... def tensor_add_with_sig(x, y):
        ...     z = x + y
        ...     return z
        ...
        >>> out = tensor_add_with_sig(x, y)
 