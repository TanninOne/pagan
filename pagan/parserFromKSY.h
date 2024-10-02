#pragma once

#include "Parser.h"

namespace pagan {

std::shared_ptr<Parser> parserFromKSY(const char *specFileName);

} // namespace pagan
