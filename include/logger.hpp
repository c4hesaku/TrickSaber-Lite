#pragma once

#include "scotland2/shared/loader.hpp"
#include "paper/shared/logger.hpp"


inline modloader::ModInfo modInfo = {MOD_ID, VERSION, 0};

Paper::ConstLoggerContext<12UL> getLogger();