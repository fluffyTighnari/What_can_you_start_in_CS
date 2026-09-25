#pragma once

#include "Parse/parse.hpp"
#include "Help/Error/error.hpp"
#include "Help/Show/show.hpp"
#include "Storage_engine/storage.hpp"
#include <string>
#include <vector>
#include <memory>

Result<std::unique_ptr<Info>> execute_command(Command* command, Storage* storage);
