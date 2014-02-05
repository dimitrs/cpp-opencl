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

class KernelFixture
{
public:
    KernelFixture()
    {
        try {
            VECTOR_CLASS<cl::Platform> Platforms;
            cl::Platform::get(&Platforms);
            if (Platforms.size() > 0) {
                Platform = Platforms[0];
                Setup();
                TransformSource();
            }
        } catch(cl::Error& e) {
            std::cerr << e.what() << ": " << e.err() << "\n";
        } catch(std::exception& e) {
            std::cerr << e.what() << "\n";
        }
    }

    static KernelFixture& Instance()
    {
        static KernelFixture I;
        return I;
    }

    std::string GetKernelCode() const { return KernelCode; }

    void BuildKernel(const std::string& Kernelname)
    {
        try {
            cl::Program::Sources Sources;
            Sources.push_back({KernelCode.c_str(),KernelCode.length()});
            Program = cl::Program(Context,Sources);
            Program.build({Device});
            Kernel = cl::Kernel(Program, Kernelname.c_str());
        } catch(cl::Error& e) {
            std::cerr << e.what() << ": " << e.err() << "\n";
            std::cerr << "Build Status: " << Program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(Device) << std::endl;
            std::cerr << "Build Options:\t" << Program.getBuildInfo<CL_PROGRAM_BUILD_OPTIONS>(Device) << std::endl;
            std::cerr << "Build Log:\t " << Program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(Device) << std::endl;
        } catch(std::exception& e) {
            std::cerr << e.what() << "\n";
        }
    }

    template<typename T, typename Val=int>
    void Run(T& In1, T& In2, T& Out)
    {
        int Extent = std::end(In1)-std::begin(In1);
        ::size_t ByteLength = sizeof(Val) * (Extent);

        cl::Buffer B1(Context,CL_MEM_READ_WRITE, ByteLength);
        Queue.enqueueWriteBuffer(B1,CL_TRUE,0,ByteLength, static_cast<void*>(&*std::begin(In1)));

        cl::Buffer B2(Context,CL_MEM_READ_WRITE, ByteLength);
        Queue.enqueueWriteBuffer(B2,CL_TRUE,0,ByteLength, static_cast<void*>(&*std::begin(In2)));

        cl::Buffer O(Context,CL_MEM_READ_WRITE,ByteLength);

        Kernel.setArg(0,B1);
        Kernel.setArg(1,B2);
        Kernel.setArg(2,O);
        Queue.enqueueNDRangeKernel(Kernel, cl::NullRange, cl::NDRange(Extent), cl::NullRange);
        Queue.finish();

        Queue.enqueueReadBuffer(O,CL_TRUE,0,ByteLength,static_cast<void*>(&*std::begin(Out)));
    }

    template<typename T, typename Val=int>
    void Run(T& Buf, cl::NDRange global=cl::NullRange, cl::NDRange local=cl::NullRange)
    {
        size_t Extent = std::end(Buf)-std::begin(Buf);
        if (global.dimensions() == 0)
            global = {Extent};
        ::size_t ByteLength = sizeof(Val) * Extent;

        cl::Buffer Buffer(Context,CL_MEM_READ_WRITE,ByteLength);
        Queue.enqueueWriteBuffer(Buffer,CL_TRUE,0,ByteLength, static_cast<void*>(&*std::begin(Buf)));
        Kernel.setArg(0,Buffer);
        Queue.enqueueNDRangeKernel(Kernel, cl::NullRange, global, local);
        Queue.finish();
        Queue.enqueueReadBuffer(Buffer,CL_TRUE,0,ByteLength,static_cast<void*>(&*std::begin(Buf)));
    }

    template<typename T, typename Val=int>
    void Run(T& Buf1, T& Buf2, cl::NDRange global=cl::NullRange, cl::NDRange local=cl::NullRange)
    {
        size_t Extent = std::end(Buf1)-std::begin(Buf1);
        if (global.dimensions() == 0)
            global = {Extent};
        ::size_t ByteLength = sizeof(Val) * Extent;

        cl::Buffer Buffer1(Context,CL_MEM_READ_WRITE,ByteLength);
        Queue.enqueueWriteBuffer(Buffer1,CL_TRUE,0,ByteLength, static_cast<void*>(&*std::begin(Buf1)));
        cl::Buffer Buffer2(Context,CL_MEM_READ_WRITE,ByteLength);
        Queue.enqueueWriteBuffer(Buffer2,CL_TRUE,0,ByteLength, static_cast<void*>(&*std::begin(Buf2)));

        Kernel.setArg(0,Buffer1);
        Kernel.setArg(1,Buffer2);
        Queue.enqueueNDRangeKernel(Kernel, cl::NullRange, global, local);
        Queue.finish();
        Queue.enqueueReadBuffer(Buffer1,CL_TRUE,0,ByteLength,static_cast<void*>(&*std::begin(Buf1)));
        Queue.enqueueReadBuffer(Buffer2,CL_TRUE,0,ByteLength,static_cast<void*>(&*std::begin(Buf2)));
    }

private:
    void Setup()
    {
        Devices = new VECTOR_CLASS<cl::Device>;
        if (CL_SUCCESS != Platform.getDevices(CL_DEVICE_TYPE_ALL, Devices) || Devices->size() == 0)
            throw std::runtime_error("Failed to create AcceleratorImpl.");
        Device = (*Devices)[0];

        std::cout << "Using platform: "<<Platform.getInfo<CL_PLATFORM_NAME>()<<"\n";
        std::cout << "Using device: "<<Device.getInfo<CL_DEVICE_NAME>()<<"\n";

        Context = cl::Context(VECTOR_CLASS<cl::Device>{Device});

        Queue = cl::CommandQueue(Context,Device);
    }

    void TransformSource()
    {
        static const char FileName[] = "kernel.cpp";

        const char* CmdLine[] = {
            "clang",
            "-x", "c++", "-std=c++11", "-O3",
            "-o", "/tmp/test.cc.o",
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
        KernelCode = compiler::MainEntry(17, CmdLine, compiler::BuildClCode)[0];

#ifdef HACK
        std::ifstream sourceFile("/tmp/opencl_temp.cl");
        if(sourceFile.fail())
            throw cl::Error(1, "Failed to open OpenCL source file");
        std::string sourceCode(
            std::istreambuf_iterator<char>(sourceFile),
            (std::istreambuf_iterator<char>()));
        KernelCode = sourceCode;
#endif


    }

private:
    VECTOR_CLASS<cl::Device>* Devices;
    cl::Platform Platform;
    cl::Device Device;
    cl::Context Context;
    cl::CommandQueue Queue;
    cl::Program Program;
    cl::Kernel Kernel;
    cl::Program::Sources Sources;
    std::string KernelCode;
};


TEST_CASE( "some cl operations", "[opencl]" ) {

    KernelFixture& K = KernelFixture::Instance();

    SECTION( "test C++ to OpenCL transformations" ) {
        REQUIRE( K.GetKernelCode()  != "" );

        SECTION( "test opencl function get_global_id" ) {
            K.BuildKernel("_Kernel_global_id");

            int Out[3];
            K.Run(Out, 3);

            REQUIRE( 0 == Out[0] );
            REQUIRE( 1 == Out[1] );
            REQUIRE( 2 == Out[2] );
        }

        SECTION( "test opencl function get_global_id for 2D data" ) {
            K.BuildKernel("_Kernel_global_id2d");

            static const int GLOBAL_DIM_X = 10;
            static const int GLOBAL_DIM_Y = 10;
            int out_data[GLOBAL_DIM_X * GLOBAL_DIM_Y];
            K.Run(out_data, {GLOBAL_DIM_X, GLOBAL_DIM_Y}, {5, 5});

            REQUIRE( 0 == out_data[0] );
            REQUIRE( 1 == out_data[1] );
            REQUIRE( 2 == out_data[2] );
            REQUIRE( 10 == out_data[10] );
            REQUIRE( 20 == out_data[20] );
            REQUIRE( 90 == out_data[90] );
            REQUIRE( 95 == out_data[95] );
            REQUIRE( 99 == out_data[99] );

            for (int i = 0; i < GLOBAL_DIM_X; i++) {
                for (int j = 0; j < GLOBAL_DIM_Y; j++) {
                    fprintf(stderr, "%2u ", out_data[i * GLOBAL_DIM_Y + j]);
                }
                fprintf(stderr, "\n");
            }
        }

        SECTION( "add two integers" ) {
            K.BuildKernel("_Kernel_add");

            int Arg1[1] = { 1 };
            int Arg2[1] = { 2 };
            int Out[1];
            K.Run(Arg1, Arg2, Out);

            REQUIRE( 3 == Out[0] );
        }

        SECTION( "test divide" ) {
            K.BuildKernel("_Kernel_div");

            int Arg1[1] = { 4 };
            int Arg2[1] = { 2 };
            int Out[1];
            K.Run(Arg1, Arg2, Out);

            REQUIRE( 2 == Out[0] );
        }

        SECTION( "test if_eq statement" ) {
            K.BuildKernel("_Kernel_if_eq");

            int Arg1[1] = { 4 };
            int Arg2[1] = { 4 };
            int Out[1] = { 0 };
            K.Run(Arg1, Arg2, Out);
            REQUIRE( 1 == Out[0] );

            int Arg3[1] = { 3 };
            int Arg4[1] = { 4 };
            Out[1] = { 0 };
            K.Run(Arg3, Arg4, Out);
            REQUIRE( 0 == Out[0] );
        }

        SECTION( "test std::enable_if return type" ) {
            K.BuildKernel("_Kernel_enable_if_return_type");

            int Arg1[1] = { 4 };
            int Out[1] = { 0 };
            K.Run(Arg1, Out);

            REQUIRE( 4 == Out[0] );
        }

        SECTION( "test std::enable_if int argument " ) {
            K.BuildKernel("_Kernel_enable_if_int_argument");

            int Arg[1] = { 1 };
            int Out[1] = { 0 };
            K.Run(Arg, Out);

            REQUIRE( 1 == Out[0] );
        }

        SECTION( "tst std::enable_if float argument" ) {
            K.BuildKernel("_Kernel_enable_if_float_argument");

            cl_float Arg[1] = { 1.0 };
            cl_float Out[1] = { 1 };
            K.Run(Arg, Out);

            REQUIRE( Approx(0.0) == Out[0] );
        }

        SECTION( "test std::find_if find an odd number" ) {
            K.BuildKernel("_Kernel_find_if");

            int Arg[3] = { 4, 3, 8 };
            int Out[3] = { 0 };
            K.Run(Arg, Out);

            REQUIRE( 3 == Out[0] );
        }

        SECTION( "test std::sort" ) {
            K.BuildKernel("_Kernel_sort");

            int Arg[8] = { 4, 3, 8, 10, 7, 8, 20, 10 };
            int Out[8] = { 0 };
            K.Run(Arg, Out);

            REQUIRE( 3 == Out[0] );
        }
    }
}


