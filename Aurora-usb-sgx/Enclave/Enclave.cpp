/*
 * Copyright (C) 2011-2017 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *   * Neither the name of Intel Corporation nor the names of its
 *     contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "sgx_trts.h"

#include "Enclave.h"
#include "Enclave_t.h"  /* print_string */

/* 
 * printf: 
 *   Invokes OCALL to display the enclave buffer to the terminal.
 */
void printf(const char *fmt, ...)
{
    char buf[BUFSIZ] = {'\0'};
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, BUFSIZ, fmt, ap);
    va_end(ap);
    ocall_print_string(buf);
}

#define BLOCK_LEN 64  // In bytes
#define STATE_LEN 4  // In words

// Link this program with an external C or x86 compression function
extern "C" {
    void md5_compress(uint32_t state[STATE_LEN], const uint8_t block[BLOCK_LEN]);
};

/* Full message hasher */

void md5_hash(const uint8_t message[], size_t len, uint32_t hash[STATE_LEN]) {
	hash[0] = UINT32_C(0x67452301);
	hash[1] = UINT32_C(0xEFCDAB89);
	hash[2] = UINT32_C(0x98BADCFE);
	hash[3] = UINT32_C(0x10325476);
	
	#define LENGTH_SIZE 8  // In bytes
	
	size_t off;
	for (off = 0; len - off >= BLOCK_LEN; off += BLOCK_LEN)
		md5_compress(hash, &message[off]);
	
	uint8_t block[BLOCK_LEN] = {0};
	size_t rem = len - off;
	memcpy(block, &message[off], rem);
	
	block[rem] = 0x80;
	rem++;
	if (BLOCK_LEN - rem < LENGTH_SIZE) {
		md5_compress(hash, block);
		memset(block, 0, sizeof(block));
	}
	
	block[BLOCK_LEN - LENGTH_SIZE] = (uint8_t)((len & 0x1FU) << 3);
	len >>= 5;
	for (int i = 1; i < LENGTH_SIZE; i++, len >>= 8)
		block[BLOCK_LEN - LENGTH_SIZE + i] = (uint8_t)(len & 0xFFU);
	md5_compress(hash, block);
}

void ecall_test(char *buffer)
{
  uint32_t pos = 0, len = 0;
  
  memset(buffer, 0, 1<<12);
  sgx_read_rand(buffer+10, (1<<12)-10);
  
//  len = snprintf((char *)(buffer + pos), 7, "AURORA");
//  pos += len;
  memcpy(buffer, "AURORA", 6);

  uint32_t hash[STATE_LEN];
  md5_hash(buffer+10, (1<<12)-10, hash);
  memcpy(buffer+6, hash, STATE_LEN);
  pos = (1<<12);

  if (pos) {
//    buffer[pos] = '\0';
//    printf("ecall_test pos=%d,buffer=%s\n", pos, buffer);
    uint32_t wLen = (pos + 4095) & ~4095U;
    ocall_write(buffer, wLen, pos);
  }
  
  return;
}
