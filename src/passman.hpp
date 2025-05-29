#pragma once

#include "passes/passes.hpp"

#include <memory>
#include <vector>

class PassMan {
private:
  std::vector<std::shared_ptr<FuncPass>> passes;

public:
  void setPasses(std::vector<std::shared_ptr<FuncPass>> newpasses) {
    passes = newpasses;
  }

  const std::vector<std::shared_ptr<FuncPass>> &getPasses() const {
    return passes;
  }
};
