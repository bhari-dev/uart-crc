#include "crc32.h"

uint32_t CRC32_Compute(const uint8_t *data, size_t length)
{
    uint32_t crc = 0xFFFFFFFFu;
    while (length--)
    {
        crc ^= (uint32_t)*data++;
        for (int i = 0; i < 8; i++)
        {
            if (crc & 1u)
                crc = (crc >> 1) ^ 0xEDB88320u; // actual crc G polynomial is 0x04C11DB7
            else
                crc = (crc >> 1);
        }
    }
    return ~crc;
}

void CRC32_Append(uint8_t *buf, uint16_t *length)
{
    uint32_t crc = CRC32_Compute(buf, *length);
    // append in big-endian order (MSB first)
    buf[(*length)++] = (uint8_t)(crc >> 24);
    buf[(*length)++] = (uint8_t)(crc >> 16);
    buf[(*length)++] = (uint8_t)(crc >> 8);
    buf[(*length)++] = (uint8_t)(crc >> 0);
}

bool CRC32_ValidateFrame(const uint8_t *buf,
                         uint16_t     buf_len,
                         uint8_t      delimiter,
                         uint16_t    *payload_len)
{
    // Must have at least: 1 byte payload + 4 CRC + 1 delimiter
    if (buf_len < 6) return false;

    // Find the last delimiter in the buffer
    int16_t idx = -1;
    for (int16_t i = buf_len - 1; i >= 0; --i) {
        if (buf[i] == delimiter) { idx = i; break; }
    }
    if (idx < 5) return false;  // need >= payload(1) + CRC4

    // Payload is everything before the CRC
    uint16_t plen = idx - 4;
    // Extract received CRC (big-endian)
    uint32_t received =  ((uint32_t)buf[plen]   << 24)
                       | ((uint32_t)buf[plen+1] << 16)
                       | ((uint32_t)buf[plen+2] <<  8)
                       | ((uint32_t)buf[plen+3] <<  0);

    // Compute CRC over payload
    uint32_t computed = CRC32_Compute(buf, plen);

    if (received == computed) {
        if (payload_len) *payload_len = plen;
        return true;
    } else {
        return false;
    }
}

void FlipBit(uint8_t *buf, uint16_t buf_len, uint16_t byte_idx, uint8_t bit_idx)
{
    if (byte_idx >= buf_len || bit_idx > 7) {}
    buf[byte_idx] ^= (1u << bit_idx);
}