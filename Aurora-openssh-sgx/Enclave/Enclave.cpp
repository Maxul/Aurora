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

void ecall_dummy() {}

int put_out_buffer(unsigned char *raw_msg, const int msg_len);
int get_in_buffer(unsigned char *message, int *message_len);

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "sodium.h"

void ecall_test()
{
    unsigned char      *ad;
    unsigned char      *ciphertext;
    unsigned char      *decrypted;
    unsigned char      *detached_ciphertext;
    unsigned char      *expected_ciphertext;
    unsigned char      *key;
    unsigned char      *message;
    unsigned char      *mac;
    unsigned char      *nonce;
    char               *hex;
    unsigned long long  found_ciphertext_len;
    unsigned long long  found_mac_len;
    unsigned long long  found_message_len;
    size_t              ad_len = 0;
    size_t              ciphertext_len;
    size_t              detached_ciphertext_len;
    size_t              i = 0U;
    size_t              message_len = 1<<12;

    ciphertext_len = message_len + crypto_aead_aes256gcm_ABYTES;

    key = (unsigned char *) sodium_malloc(crypto_aead_aes256gcm_KEYBYTES);
    nonce = (unsigned char *) sodium_malloc(crypto_aead_aes256gcm_NPUBBYTES);
    mac = (unsigned char *) sodium_malloc(crypto_aead_aes256gcm_ABYTES);

    message = (unsigned char *) sodium_malloc(message_len);
    ad = (unsigned char *) sodium_malloc(ad_len);
    ciphertext = (unsigned char *) sodium_malloc(ciphertext_len);
    decrypted = (unsigned char *) sodium_malloc(message_len);

    memset(key, 0xf2, crypto_aead_aes256gcm_KEYBYTES);
    memset(message, 'B', message_len);

#define N	10000
for (i = 0; i < N; i++) {

    crypto_aead_aes256gcm_encrypt(ciphertext, &found_ciphertext_len,
                                      message, message_len,
                                      ad, ad_len, NULL, nonce, key);

    found_message_len = 1;
    if (crypto_aead_aes256gcm_decrypt(decrypted, &found_message_len,
                                          NULL, ciphertext, ciphertext_len,
                                          ad, ad_len, nonce, key) != 0) {
        printf("Verification of test vector #%u failed\n", (unsigned int) i);
    }

}

    sodium_free(message);
    sodium_free(ad);
    sodium_free(ciphertext);
    sodium_free(decrypted);

    sodium_free(key);
    sodium_free(mac);
    sodium_free(nonce);
}

#define MAX_SMI_CNT 1

void try_trigger_smi()
{
    static volatile short counter = 0;

    counter ++;
    if (counter >= MAX_SMI_CNT) {
        ocall_smi();
        counter = 0;
    }
}

unsigned char *uio_ram = nullptr;

void ecall_aurora_init(unsigned char *uio_mem)
{
    uio_ram = uio_mem;
}

void _print_hex(const char* what, const void* v, const unsigned long l);

void ecall_secure_input(unsigned char *pass, int max_length)
{
    ocall_smi();
#define PKT_SIZE  (1 << 12)
#define MSG_SIZE  (PKT_SIZE - 16)

    ocall_uio_poll();
    
    unsigned char recv_msg[PKT_SIZE];
    int min_len = -1, recv_len = 0;
    memset(recv_msg, 0, sizeof(recv_msg));
    while (0 == get_in_buffer(recv_msg, &recv_len)) {
        //_print_hex("keyboard test", recv_msg, MSG_SIZE);
    }
    min_len = strlen((char *)recv_msg);
    if (max_length < min_len)
        min_len = max_length;
    memcpy(pass, recv_msg, min_len);
//    printf("%d\n", strlen((char *)recv_msg));
//    printf("%s\n", recv_msg);
}

