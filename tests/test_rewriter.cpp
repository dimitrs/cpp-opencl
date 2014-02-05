#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main()
#include "../tests/catch.h"

#include <vector>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <stdexcept>

#define __CL_ENABLE_EXCEPTIONS
#include "cl.h"

#include "../sources/compiler/MainEntry.h"
#include "../sources/compiler/Compiler.h"


std::vector<std::string> TransformSource(std::string Source)
{
    static const char FileName[] = "Input.cpp";

    std::ofstream Afile {FileName};
    Afile << Source;
    Afile.close();

    const char* CmdLine[] = {
        "clang",
        "-x", "c++", "-std=c++11", "-O3",
        "-o", "Input.cc.o",
        "-I/home/dimitri/projects/Clang/amp/install/lib/clang/3.4/include",
        "-I/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../../include/c++/4.7",
        "-I/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../../include/c++/4.7/x86_64-linux-gnu",
        "-I/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../../include/c++/4.7/backward",
        "-I/usr/lib/gcc/x86_64-linux-gnu/4.7/../../../../include/c++/4.7/bits",
        "-I/usr/local/include",
        "-I/usr/include/x86_64-linux-gnu",
        "-I/usr/include",
        "-c", FileName
    };
    auto Code = compiler::MainEntry(17, CmdLine, compiler::RewriteSourceFile);
    assert(2 == Code.size());
    return Code;
}

static inline std::string& remove_whitespace(std::string &s) {
    s.erase(std::remove_if(s.begin(), s.end(), isspace), s.end());
    return s;
}

void CheckRewritenSource(std::string Cpu1, std::string Cpu2, std::string Gpu1, std::string Gpu2) {
    remove_whitespace(Cpu1);
    remove_whitespace(Cpu2);
    remove_whitespace(Gpu1);
    remove_whitespace(Gpu2);
    REQUIRE( Cpu1 == Cpu2 );
    REQUIRE( Gpu1 == Gpu2 );
}

TEST_CASE( "opencl rewriter", "[rewriter]" ) {

    SECTION( "Add modifier to const member function" ) {
        const char* CpuCode = R"(
          struct FooClass {
            int func(int x) const volatile __attribute__((amp_restrict("cpu")));
          };
          int FooClass::func(int x)  const volatile __attribute__((amp_restrict("cpu"))) {
            return x;
          }
          int main() {
            FooClass f;
            int (  FooClass::* pointer)( int x)  const volatile __attribute__((amp_restrict("cpu"))) = &FooClass::func;
            return ((f).*(pointer))( 5) ==  12 ? 0 : 1;
          }
        )";

        std::string Gpucode = "struct FooClass {};";

        auto Code = TransformSource(CpuCode);
        auto CpuSource = Code[0];
        auto GpuSource = Code[1];

        CheckRewritenSource(CpuSource, CpuCode, GpuSource, Gpucode);
    }

    SECTION( "parallel_for_each" ) {
        const char* InputCode = R"(
          #include <vector>
          #include "ParallelForEach.h"

          template<class T> T square(T x)  __attribute__((amp_restrict("gpu","cpu")))
          { return x * x; }

          void func() {
            std::vector<int> MyArray {1,2,3,4,5,6};
            std::vector<int> Output(6);

            compute::parallel_for_each(MyArray.begin(), MyArray.end(), Output.begin(), [](int x) {
              return square(x);
            });
          }

          int main() {
            func();
            return 0;
          }
        )";

        const char* CpuCode = R"(
          #include <vector>
          #include "ParallelForEach.h"

          template<class T> T square(T x)  __attribute__((amp_restrict("gpu","cpu")))
          { return x * x; }

          void func() {
            std::vector<int> MyArray {1,2,3,4,5,6};
            std::vector<int> Output(6);

            compute::parallel_for_each(MyArray.begin(), MyArray.end(), Output.begin(), [](int x)  {
              return std::pair<std::string,std::string> (  "Input.cpp.cl" , "_Kernel_1804289383" );
            });
          }

          int main() {
            func();
            return 0;
          }
        )";

        const char* GpuCode = R"(
          #include <vector>

          template<class T> T square(T x)  __attribute__((amp_restrict("gpu","cpu")))
          { return x * x; }

          void func() {
            std::vector<int> MyArray {1,2,3,4,5,6};
            std::vector<int> Output(6);
          }

          extern "C" long long get_global_id(int);
          extern "C" int get_global_size(int);

          int _Lambda_1804289383(int x) { return square(x); }
          extern "C" void _Kernel_1804289383(int* in, int* out) { unsigned idx = get_global_id(0); out[idx] = _Lambda_1804289383(in[idx]); }
        )";

        auto Code = TransformSource(InputCode);
        auto CpuSource = Code[0];
        auto GpuSource = Code[1];

        CheckRewritenSource(CpuSource, CpuCode, GpuSource, GpuCode);
    }

    SECTION( "Overload member function" ) {
        const char* InputCode = R"(
          struct A {
            int doubleIt() const __attribute__((amp_restrict("cpu"))) {
                return 2;
            }
            int doubleIt() const __attribute__((amp_restrict("gpu"))) {
                return 4;
            }
          };
        )";

        const char* CpuCode = R"(
          struct A {
            int doubleIt() const __attribute__((amp_restrict("cpu"))) {
                return 2;
            }
          };
        )";

        const char* GpuCode = R"(
          struct A {
            int doubleIt() const __attribute__((amp_restrict("gpu"))) {
                return 4;
            }
          };
        )";

        auto Code = TransformSource(InputCode);
        auto CpuSource = Code[0];
        auto GpuSource = Code[1];

        CheckRewritenSource(CpuSource, CpuCode, GpuSource, GpuCode);
    }
}


