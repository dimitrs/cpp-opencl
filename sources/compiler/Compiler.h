#ifndef COMPILER_H
#define COMPILER_H

#include <string>
#include <vector>
#include <clang/Basic/LLVM.h>

namespace compiler {

std::vector<std::string> RewriteSourceFile(clang::SmallVector<const char*, 256>& Args);
std::vector<std::string> BuildClCode(clang::SmallVector<const char*, 256>& Args);


}


#endif
