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

Function Overloading 
--------------------

Additionally, it is possible to overload functions. The “A::GetIt” member function below is overloaded. The function marked as “gpu” will be executed on the GPU and other on the CPU.

```
struct A {
  int GetIt() const __attribute__((amp_restrict("cpu"))) {
    return 2;
  }
  int GetIt() const __attribute__((amp_restrict("gpu"))) {
    return 4;
  }
};

compute::parallel_for_each(In.begin(), In.end(), OutGpu.begin(), [](int x){
    A a; 
    return a.GetIt(); // returns 4
});
```

If you want to use function overloading using the amp_restrict attribute, you will need to patch your Clang compiler:

git clone https://github.com/llvm-mirror/clang.git
cd clang
git checkout 5806bb59d2d19a9b32b739589865d8bb1e2627c5
git apply PATH-TO-cpp_opencl/restrict.patch

I used this llvm version:

git clone https://github.com/llvm-mirror/llvm.git
cd llvm
git checkout 47042bcc266285676f8ff284e5d46a2c196c367b

You can use any recent Clang version already installed on your machine (without the patch), if you do not intend to use the amp_restrict attribute. 


Build the Executable 
--------------------

The tool uses a special compiler based on Clang/LLVM. 

cpp_opencl -x c++ -std=c++11 -O3 -o Input.cc.o -c Input.cc 

The above command generates four files: 
1. Input.cc.o 
2. Input.cc.cl 
3. Input.cc_cpu.cpp 
4. Input.cc_gpu.cpp 

Use the Clang C++ compiler directly to link: 

clang++ ./Input.cc.o -o test -lOpenCL 


Then just execute: 

./test
