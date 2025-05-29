#pragma once

#include "llvm/IR/Function.h"

#include <string>

class FuncPass {
public:
  virtual ~FuncPass() = default;
  virtual void run(llvm::Function &func) = 0;
  virtual std::string name() const = 0;
};

class LivenessAnalysis : public FuncPass {
public:
  void run(llvm::Function &func) override;
  std::string name() const override { return "liveness"; }
};

class Points2Analysis : public FuncPass {
public:
  void run(llvm::Function &func) override;
  std::string name() const override { return "points-to"; }
};

class Slicing : public FuncPass {
public:
  void run(llvm::Function &func) override;
  std::string name() const override { return "slicing"; }
};

class ZeroCFAnalysis : public FuncPass {
public:
  void run(llvm::Function &func) override;
  std::string name() const override { return "0-CFA"; }
};
