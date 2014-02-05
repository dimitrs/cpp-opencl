cpp-opencl
==========

Please see http://dimitri-christodoulou.blogspot.com.es/2014/02/implement-data-parallelism-on-gpu.html


The cpp-opencl project provides a way to make programming GPUs easy for the developer. It allows you to implement data parallelism on a GPU directly in C++ instead of using OpenCL. See the example below. The code in the parallel_for_each lambda function is executed on the GPU, and all the rest is executed on the CPU. More specifically, the “square” function is executed both on the CPU (via a call to std::transform) and the GPU (via a call to compute::parallel_for_each). Conceptually, compute::parallel_for_each is similar to std::transform except that one executes code on the GPU and the other on the CPU.

```
#include <vector>
#include <stdio.h>
#include "ParallelForEach.h"

template<class T> 
T square(T x)  
{
    return x * x;
}

void func() {
  std::vector<int> In {1,2,3,4,5,6};
  std::vector<int> OutGpu(6);
  std::vector<int> OutCpu(6);

  compute::parallel_for_each(In.begin(), In.end(), OutGpu.begin(), [](int x){
      return square(x);
  });

  
  std::transform(In.begin(), In.end(), OutCpu.begin(), [](int x) {
    return square(x);
  });

  // 
  // Do something with OutCpu and OutGpu …..........

  //

}

int main() {
  func();
  return 0;
}
```
