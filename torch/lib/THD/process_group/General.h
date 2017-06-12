#pragma once

#include "../THD.h"
#include <string>

THD_API void THDProcessGroupInit(THDChannelType channel_type, std::string init_method,
                                 int world_size, std::string group_name, int rank);

