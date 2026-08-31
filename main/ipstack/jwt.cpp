#include "jwt.h"

// Helper to convert standard Base64 to Base64URL
void convert_to_base64url(char *str) {
    char *p = str;
    while (*p) {
        if (*p == '+') *p = '-';
        else if (*p == '/') *p = '_';
        else if (*p == '=') {
            *p = '\0'; // Strip padding
            break;
        }
        p++;
    }
}

// Fixed-buffer Base64URL Encoder
int base64url_encode(const unsigned char *src, size_t src_len, char *dst, size_t dst_len) {
    size_t written = 0;
    // Standard mbedTLS base64 encode
    int ret = mbedtls_base64_encode((unsigned char *)dst, dst_len, &written, src, src_len);
    if (ret != 0) return -1; // Buffer too small or error
    
    convert_to_base64url(dst);
    return 0;
}

int generate_jwt(char *output_buf, size_t output_max_len, const char *secret, const char *device_id/*, uint32_t expiry*/) {
    char header_b64[64] = {0};
    char payload_b64[256] = {0};
    char raw_payload[128] = {0};
    
    // 1. Static Header (HS256)
    const char *header_json = "{\"alg\":\"HS256\",\"typ\":\"JWT\"}";
    if (base64url_encode((const unsigned char*)header_json, strlen(header_json), header_b64, sizeof(header_b64)) != 0) {
        return -1;
    }

    // 2. Format Payload (No dynamic JSON library needed for simple structures)
    snprintf(raw_payload, sizeof(raw_payload), "{\"device\":\"%s\"}", device_id);
    if (base64url_encode((const unsigned char*)raw_payload, strlen(raw_payload), payload_b64, sizeof(payload_b64)) != 0) {
        return -2;
    }

    // 3. Assemble the signing input (Header + '.' + Payload)
    int printed = snprintf(output_buf, output_max_len, "%s.%s", header_b64, payload_b64);
    if (printed < 0 || (size_t)printed >= output_max_len) return -1; // Overflow protection

    // 4. Crypto Sign (HMAC-SHA256 via mbedTLS)
    unsigned char hmac_result[32]; // SHA256 yields 32 bytes
    const mbedtls_md_info_t *md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    int ret = mbedtls_md_hmac(md_info, 
                             (const unsigned char *)secret, strlen(secret), 
                             (const unsigned char *)output_buf, strlen(output_buf), 
                             hmac_result);
    if (ret != 0) return -3;

    // 5. Encode the Signature
    char sig_b64[64] = {0};
    if (base64url_encode(hmac_result, sizeof(hmac_result), sig_b64, sizeof(sig_b64)) != 0) {
        return -4;
    }

    // 6. Append Signature to the JWT
    size_t current_len = strlen(output_buf);
    printed = snprintf(output_buf + current_len, output_max_len - current_len, ".%s", sig_b64);
    if (printed < 0 || (current_len + printed) >= output_max_len) return -1;

    return 0; // Success!
}

// int main() {
//     char jwt_buffer[512] = {0};
//     const char *secret_key = "super-secret-key";
    
//     if (generate_jwt(jwt_buffer, sizeof(jwt_buffer), secret_key, "sensor-node-01", 1781164800) == 0) {
//         printf("Generated JWT:\n%s\n", jwt_buffer);
//     } else {
//         printf("Failed to generate JWT.\n");
//     }
//     return 0;
// }