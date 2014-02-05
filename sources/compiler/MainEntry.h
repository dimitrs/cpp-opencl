#ifndef MAINENTRY_H
#define MAINENTRY_H

#include <vector>
#include <string>
#include <functional>

#include <clang/Basic/LLVM.h>

namespace compiler {

std::vector<std::string> MainEntry(int Argc,
                                   const char** Argv,
                                   std::function<std::vector<std::string>(clang::SmallVector<const char*, 256>&)> F);

}


#endif
