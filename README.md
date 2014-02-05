cpp-opencl
==========

C++ to OpenCL C Source-to-source Translation

Please see http://dimitri-christodoulou.blogspot.com/2013/12/writing-opencl-kernels-in-c.html


The tool compiles the C++ source into LLVM byte-code, and uses a modified version of the LLVM 'C' back-end to disassemble the byte-code into OpenCL 'C'.

For example, the following code using C++11's std::enable_if can be compiled and executed on the GPU:


```

#include <type_traits>

template<class T>
T foo(T t, typename std::enable_if<std::is_integral<T>::value >::type* = 0)
{
    return 1;
}

template<class T>
T foo(T t, typename std::enable_if<std::is_floating_point<T>::value >::type* = 0)
{
    return 0;
}

extern "C" void _Kernel_enable_if_int_argument(int* arg0, int* out)
{
    out[0] = foo(arg0[0]);
}

```
