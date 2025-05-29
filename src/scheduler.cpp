#include "scheduler.hpp"
#include "passes/passes.hpp"

#include "llvm/IR/Module.h"
#include "llvm/Support/raw_ostream.h"

using namespace llvm;

void Sequential::run(const std::vector<std::shared_ptr<FuncPass>> &passes,
                     Module &module) {
  for (auto pass : passes) {
    outs() << pass->name() << " ";
    for (Function &func : module) {
      if (func.isDeclaration())
        continue;
      pass->run(func);
    }
  }
  outs() << "\n";
}

void ConcurrentPasses::run(const std::vector<std::shared_ptr<FuncPass>> &passes,
                           Module &module) {
  //
}

void ConcurrentFuncs::run(const std::vector<std::shared_ptr<FuncPass>> &passes,
                          Module &module) {
  //
}
