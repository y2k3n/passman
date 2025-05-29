#include "passes/passes.hpp"
#include "passman.hpp"
#include "scheduler.hpp"

#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/JSON.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

#include <chrono>
#include <queue>
// #include <set>
// #include <unordered_map>
#include <cmath>
#include <fstream>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

using namespace llvm;

int main(int argc, char *argv[]) {
  InitLLVM X(argc, argv);
  if (argc < 2) {
    errs() << "Expect IR filename\n";
    return 1;
  }
  char *filename = argv[1];

  LLVMContext context;
  SMDiagnostic smd;
  std::unique_ptr<Module> module = parseIRFile(filename, smd, context);
  if (!module) {
    outs() << "Cannot parse IR file\n";
    smd.print(filename, outs());
    exit(1);
  }

  PassMan passman;
  passman.setPasses({
      std::make_shared<LivenessAnalysis>(),
      std::make_shared<Points2Analysis>(),
      std::make_shared<ZeroCFAnalysis>(),
      std::make_shared<Slicing>(),
  });

  Sequential sequential;
  outs() << "Sequential: " << module->getModuleIdentifier() << "\n";
  auto start = std::chrono::high_resolution_clock::now();
  sequential.run(passman.getPasses(), *module);
  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  outs() << "Analysis time: " << duration.count() << " us\n";

  ConcurrentPasses concurrentPasses;
  outs() << "Passes concurrently: " << module->getModuleIdentifier()
         << "\n";
  start = std::chrono::high_resolution_clock::now();
  concurrentPasses.run(passman.getPasses(), *module);
  end = std::chrono::high_resolution_clock::now();
  duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  outs() << "Analysis time: " << duration.count() << " us\n";

  ConcurrentFuncs concurrentFuncs;
  outs() << "Funcs concurrently: " << module->getModuleIdentifier()
         << "\n";
  start = std::chrono::high_resolution_clock::now();
  concurrentFuncs.run(passman.getPasses(), *module);
  end = std::chrono::high_resolution_clock::now();
  duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  outs() << "Analysis time: " << duration.count() << " us\n";
}
