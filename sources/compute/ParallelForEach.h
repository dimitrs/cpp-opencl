#ifndef ParallelForEach_H
#define ParallelForEach_H

#define __CL_ENABLE_EXCEPTIONS
#include "cl.h"

#include <iostream>
#include <fstream>
#include <stdexcept>

namespace compute {


class Accelerator
{
public:
    Accelerator()
    {
        try {
            VECTOR_CLASS<cl::Platform> Platforms;
            cl::Platform::get(&Platforms);
            if (Platforms.size() > 0) {
                Platform = Platforms[0];
                Setup();
            }
        } catch(cl::Error& e) {
            std::cerr << e.what() << ": " << e.err() << "\n";
        } catch(std::exception& e) {
            std::cerr << e.what() << "\n";
        }
    }

    Accelerator(const Accelerator& that) = delete;
    Accelerator& operator=(Accelerator&) = delete;

    static Accelerator& Instance()
    {
        static Accelerator I;
        return I;
    }

    void BuildKernel(const std::string& KernelName, const std::string& KernelCode)
    {
        try {
            cl::Program::Sources Sources;
            Sources.push_back({KernelCode.c_str(),KernelCode.length()});
            Program = cl::Program(Context,Sources);
            Program.build({Device});
            Kernel = cl::Kernel(Program, KernelName.c_str());
        } catch(cl::Error& e) {
            std::cerr << e.what() << ": " << e.err() << "\n";
            std::cerr << "Build Status: " << Program.getBuildInfo<CL_PROGRAM_BUILD_STATUS>(Device) << std::endl;
            std::cerr << "Build Options:\t" << Program.getBuildInfo<CL_PROGRAM_BUILD_OPTIONS>(Device) << std::endl;
            std::cerr << "Build Log:\t " << Program.getBuildInfo<CL_PROGRAM_BUILD_LOG>(Device) << std::endl;
        } catch(std::exception& e) {
            std::cerr << e.what() << "\n";
        }
    }

    template <typename InputIterator, typename OutputIterator>
    void Run(InputIterator begin, InputIterator end, OutputIterator output)
    {
        typedef typename std::iterator_traits<InputIterator>::value_type value_type;
        int Extent = std::distance(begin, end);
        ::size_t ByteLength = sizeof(value_type) * (Extent);

        cl::Buffer BufferIn(Context,CL_MEM_READ_WRITE, ByteLength);
        Queue.enqueueWriteBuffer(BufferIn,CL_TRUE,0,ByteLength, static_cast<void*>(&*begin));

        cl::Buffer BufferOut(Context,CL_MEM_READ_WRITE,ByteLength);

        Kernel.setArg(0,BufferIn);
        Kernel.setArg(1,BufferOut);
        Queue.enqueueNDRangeKernel(Kernel, cl::NullRange, cl::NDRange(Extent), cl::NullRange);
        Queue.finish();

        Queue.enqueueReadBuffer(BufferOut,CL_TRUE,0,ByteLength,static_cast<void*>(&*output));
    }


private:
    void Setup()
    {
        Devices = new VECTOR_CLASS<cl::Device>;
        if (CL_SUCCESS != Platform.getDevices(CL_DEVICE_TYPE_ALL, Devices) || Devices->size() == 0)
            throw std::runtime_error("Failed to create Accelerator.");
        Device = (*Devices)[0];

        std::cout << "Using platform: " << Platform.getInfo<CL_PLATFORM_NAME>()<<"\n";
        std::cout << "Using device: " << Device.getInfo<CL_DEVICE_NAME>()<<"\n";

        Context = cl::Context(VECTOR_CLASS<cl::Device>{Device});

        Queue = cl::CommandQueue(Context,Device);
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
};


template <typename InputIterator, typename OutputIterator, typename KernelType>
void parallel_for_each(InputIterator begin, InputIterator end, OutputIterator output, const KernelType& F)
{
    std::pair<std::string,std::string> Names = F(0);
    std::ifstream sourceFile(Names.first);
    if(sourceFile.fail())
        throw std::runtime_error("Failed to open OpenCL source file.");
    std::string KernelCode(
        std::istreambuf_iterator<char>(sourceFile),
        (std::istreambuf_iterator<char>()));

    Accelerator& K = Accelerator::Instance();
    K.BuildKernel(Names.second, KernelCode);
    K.Run(begin, end, output);
}


} // namespace compute

#endif
