#pragma once
#include <cxxtools/pool.h>
#include "fty_common_mlm_tntmlm.h"

typedef cxxtools::Pool<MlmClient> MlmClientPool;
extern MlmClientPool mlm_pool;
