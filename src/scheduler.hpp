#pragma once

#include "passman.hpp"

#include "llvm/IR/Module.h"

#include <cassert>
#include <memory>
#include <vector>

class Scheduler {
public:
  virtual ~Scheduler() = default;
  virtual void run(const std::vector<std::shared_ptr<FuncPass>> &passes,
                   llvm::Module &module) = 0;
};

class TaskTimer : public Scheduler {
public:
  void run(const std::vector<std::shared_ptr<FuncPass>> &passes,
           llvm::Module &module) override;
};

class Sequential : public Scheduler {
public:
  void run(const std::vector<std::shared_ptr<FuncPass>> &passes,
           llvm::Module &module) override;
};

class ConcurrentModules : public Scheduler {
private:
  unsigned nthreads;

public:
  ConcurrentModules() : nthreads(4) {}
  explicit ConcurrentModules(unsigned num_threads) : nthreads(num_threads) {}
  void run(const std::vector<std::shared_ptr<FuncPass>> &passes,
           llvm::Module &module) {
    exit(1);
  }
  void runOnFile(const std::vector<std::shared_ptr<FuncPass>> &passes,
                 const std::string &filename);
};

class ConcurrentPasses : public Scheduler {
public:
  void run(const std::vector<std::shared_ptr<FuncPass>> &passes,
           llvm::Module &module) override;
};

class ConcurrentFuncs : public Scheduler {
private:
  unsigned nthreads;

public:
  ConcurrentFuncs() : nthreads(4) {}
  explicit ConcurrentFuncs(unsigned num_threads) : nthreads(num_threads) {}
  void run(const std::vector<std::shared_ptr<FuncPass>> &passes,
           llvm::Module &module) override;
};

class ConcurrentTasks : public Scheduler {
private:
  unsigned nthreads;

public:
  ConcurrentTasks() : nthreads(4) {}
  explicit ConcurrentTasks(unsigned num_threads) : nthreads(num_threads) {}
  void run(const std::vector<std::shared_ptr<FuncPass>> &passes,
           llvm::Module &module) override;
};
