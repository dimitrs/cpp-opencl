#include <type_traits>
#include <algorithm>
#include <iterator>


extern "C" long long get_global_id(int);
extern "C" int get_global_size(int);

extern "C" {

void _Kernel_global_id(int* out)
{
  unsigned id = get_global_id(0);
  out[id] = id;
}

void _Kernel_global_id2d(int* out)
{
   unsigned x = get_global_id(0);
   unsigned y = get_global_id(1);
   unsigned id = (y * get_global_size(1)) + x;
   out[id] = id;
}

void _Kernel_add(int* arg0, int* arg1, int* out)
{
    out[0] = arg0[0] + arg1[0];
}

void _Kernel_div(int* arg0, int* arg1, int * out)
{
    out[0] = arg0[0] / arg1[0];
}

void _Kernel_if_eq(int* arg0, int* arg1, int* out)
{
   out[0] = 0;
   if (arg0[0] == arg1[0]) {
      out[0] = 1;
   }
}

}


// foo1 overloads are enabled via the return type
template<class T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type
foo1(T t)
{
    return t;
}

template<class T>
typename std::enable_if<std::is_integral<T>::value, T>::type
foo1(T t)
{
    return t;
}

template<class T>
T foo2(T t, typename std::enable_if<std::is_integral<T>::value >::type* = 0)
{
    return 1;
}

template<class T>
T foo2(T t, typename std::enable_if<std::is_floating_point<T>::value >::type* = 0)
{
    return 0;
}

extern "C" {

void _Kernel_enable_if_return_type(int* arg0, int* out)
{
    out[0] = foo1(arg0[0]);
}

void _Kernel_enable_if_int_argument(int* arg0, int* out)
{
    out[0] = foo2(arg0[0]);
}

void _Kernel_enable_if_float_argument(float* arg0, float* out)
{
    out[0] = foo2(arg0[0]);
}

}


extern "C" void _Kernel_find_if(int* arg, int* out) {
    int* it = std::find_if (arg, arg+3, [] (int i) { return ((i%2)==1); } );
    out[0] = *it;
}

extern "C" void _Kernel_sort(int* arg, int* out) {
    std::sort (arg, arg+3);
    out[0] = arg[0];
}

