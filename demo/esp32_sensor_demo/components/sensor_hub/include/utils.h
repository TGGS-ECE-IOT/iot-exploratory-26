#ifndef UTILS_H
#define UTILS_H

#include <stddef.h>
#include <stdint.h>

uint64_t now_ms(void);
void get_iso_time(char *out, size_t out_len);
float raw_to_pct(int raw);

#endif
