


#include "compiler/MainEntry.h"
#include "compiler/Compiler.h"


int main(int Argc, const char **Argv)
{
    compiler::MainEntry(Argc, Argv, compiler::BuildClCode);

    return 0;
}
