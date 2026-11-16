#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief  Compute CRC-32 (IEEE-802.3) over a byte buffer
 * @param  data    pointer to buffer
 * @param  length  number of bytes in data
 * @retval 32-bit CRC
 */
uint32_t CRC32_Compute(const uint8_t *data, size_t length);

/**
 * @brief  Append a 4-byte CRC to the end of a buffer
 * @param  buf     byte buffer (must have room for +4 bytes)
 * @param  length  pointer to current length; will be incremented by 4
 */
void CRC32_Append(uint8_t *buf, uint16_t *length);

bool CRC32_ValidateFrame(const uint8_t *buf,
                         uint16_t     buf_len,
                         uint8_t      delimiter,
                         uint16_t    *payload_len);


/**
 * @brief  Flip a single bit in a buffer
 * @param  buf         pointer to the buffer
 * @param  buf_len     total length of the buffer
 * @param  byte_idx    which byte to corrupt (0..buf_len-1)
 * @param  bit_idx     which bit to flip (0..7)
 */
void FlipBit(uint8_t *buf, uint16_t buf_len, uint16_t byte_idx, uint8_t bit_idx);

#endif /* CRC32_H */
