#ifndef COMPILER_H
#define COMPILER_H

#include <string>
#include <clang/Basic/LLVM.h>

namespace compiler {

std::string BuildClCode(
        const char **ArgBegin, const char **ArgEnd,
        const char *Argv0, void *MainAddr,
        clang::SmallVector<const char*, 256>& Args);


}


#endif
