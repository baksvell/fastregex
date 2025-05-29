// regex_cache.h
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void fastregex_clear_cache();
size_t fastregex_cache_size();
const char* fastregex_cache_stats();

#ifdef __cplusplus
}
#endif