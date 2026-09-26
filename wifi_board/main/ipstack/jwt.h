#ifndef JWT_H
#define JWT_H

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "esp_tls.h"
#include "mbedtls/md.h"
#include "mbedtls/base64.h"

int generate_jwt(char *output_buf, size_t output_max_len, const char *secret, const char *device_id/*, uint32_t expiry*/);

#endif