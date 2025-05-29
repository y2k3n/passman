#pragma once

#include "passman.hpp"

#include "llvm/IR/Module.h"

#include <memory>
#include <vector>

class Scheduler {
public:
  virtual ~Scheduler() = default;
  virtual void run(const std::vector<std::shared_ptr<FuncPass>> &passes,
                   llvm::Module &module) = 0;
};

class Sequential : public Scheduler {
public:
  void run(const std::vector<std::shared_ptr<FuncPass>> &passes,
           llvm::Module &module) override;
};

class ConcurrentPasses : public Scheduler {
public:
  void run(const std::vector<std::shared_ptr<FuncPass>> &passes,
           llvm::Module &module) override;
};

class ConcurrentFuncs : public Scheduler {
public:
  void run(const std::vector<std::shared_ptr<FuncPass>> &passes,
           llvm::Module &module) override;
};
