#include <stdarg.h>
#include <string.h>
#include <stdio.h>      /* vsnprintf */

#include "Enclave.h"
#include "Enclave_t.h"  /* print_string */
#include "sgx_trts.h"

#include "sodium.h"

void _print_info();
void checkpoint(const char *what);
void _print_hex(const char* what, const void* v, const unsigned long l);

/**
    In/Out Ring Buffer ownership
*/

#define PKT_SIZE  (1 << 12)
#define MSG_SIZE  (PKT_SIZE - 16)

#define IN_SMM  0x00
#define IN_SGX  0x01
#define OUT_SMM 0x02
#define OUT_SGX 0x03

// Currently we only set 64 entries for one buffer
#define MAX_ENTRY (64)

struct metadata {
    char in_bitmap[MAX_ENTRY];
    char out_bitmap[MAX_ENTRY];
    char reserved[PKT_SIZE - 2*MAX_ENTRY];
};

extern unsigned char *uio_ram;

/**
    This method will encrypt the raw message inside SGX EPC,
    then search for available OUT buffer and put it there.
    The out_bit will be marked at the end.
*/
int put_out_buffer(unsigned char *raw_msg, const int msg_len)
{
    unsigned char      message[MSG_SIZE];
    unsigned char      ciphertext[PKT_SIZE];
    unsigned char      *ad = (unsigned char *)"";
    unsigned char      nonce[crypto_aead_aes256gcm_NPUBBYTES];
    unsigned char      key[crypto_aead_aes256gcm_KEYBYTES];
    size_t             ad_len = 0;
    unsigned long long found_ciphertext_len;

    int i;
    unsigned char *pos;
    struct metadata *p_meta = (struct metadata *)uio_ram;
    
    // the message must come from enclave
    if (0 == sgx_is_within_enclave(raw_msg, msg_len)) {
        return -1;
    }
    
    if (msg_len < 0)
        return -1;

    // too large. TODO: fragment
    if (MSG_SIZE < msg_len)
        return -1;

    memcpy(message, raw_msg, msg_len);
    // pad if short
    if (MSG_SIZE > msg_len) {
        randombytes_buf(message + msg_len, MSG_SIZE - msg_len);
    }
#if 1
    // here we use 0xca pattern as a temporary key
    // the key will be obtained in remote attestation phase
    memset(key, 0xca, crypto_aead_aes256gcm_KEYBYTES);
    memset(nonce, 0xff, crypto_aead_aes256gcm_NPUBBYTES);

    crypto_aead_aes256gcm_encrypt(ciphertext, &found_ciphertext_len,
                                      message, MSG_SIZE,
                                      ad, ad_len, NULL, nonce, key);
#endif
checkpoint("EPC encryption done");

    // we then copy the encrypted message into the IN buffer
    for (i = 0; i < MAX_ENTRY; ++i) {
        if (OUT_SGX == p_meta->out_bitmap[i])
            break;
    }

    if (MAX_ENTRY <= i) {
        return -1;
    }
    // skip the metadata block
    pos = uio_ram + sizeof(struct metadata);
    pos += i * (PKT_SIZE);
    memcpy(pos, ciphertext, PKT_SIZE);

    // turn the ownership bit at the end
    p_meta->out_bitmap[i] = OUT_SMM;

checkpoint("Copy to shared RAM done");

    return 0;
}

/**
    This method will get an encrypted message via untrusted memory from SMM,
    copy it into SGX EPC and then decrypt it,
    finally stores the readily raw message pointer in the parameter.

    Note: the caller should provide enough size in case of overflow!!!
*/
int get_in_buffer(unsigned char *message, int *message_len)
{
    unsigned char      ciphertext[PKT_SIZE];
    unsigned char      *ad = (unsigned char *)"";
    unsigned char      nonce[crypto_aead_aes256gcm_NPUBBYTES];
    unsigned char      key[crypto_aead_aes256gcm_KEYBYTES];
    size_t             ad_len = 0;
    unsigned long long found_message_len = 0;

    // the plaintext must reside within EPCs
    if (0 == sgx_is_within_enclave(message, MSG_SIZE)) {
        return -1;
    }

    int i;
    unsigned char *pos;
    struct metadata *p_meta = (struct metadata *)uio_ram;

    // search for messages to be proccessed
    for (i = 0; i < MAX_ENTRY; ++i) {
        if (IN_SGX == p_meta->in_bitmap[i])
            break;
    }

    if (MAX_ENTRY <= i) {
        return -1;
    }

    // skip the metadata block
    pos = uio_ram + sizeof(struct metadata);
    pos += (MAX_ENTRY + i) * (PKT_SIZE);

    // copy the encrypted message from the OUT buffer
    memcpy(ciphertext, pos, PKT_SIZE);

checkpoint("Copy to EPC done");

#if 1
    // here we use 0xca pattern as a temporary key
    // the key will be obtained in remote attestation phase
    memset(key, 0xca, crypto_aead_aes256gcm_KEYBYTES);
    memset(nonce, 0xff, crypto_aead_aes256gcm_NPUBBYTES);

    if (crypto_aead_aes256gcm_decrypt(message, &found_message_len,
                                          NULL, ciphertext, PKT_SIZE,
                                          ad, ad_len, nonce, key) != 0) {
        printf("Verification of test vector #%u failed\n", (unsigned int) i);
        return -1;
    }
#endif
    *message_len = found_message_len;

    // turn the ownership bit at the end
    p_meta->in_bitmap[i] = IN_SMM;

checkpoint("EPC decryption done");

    return 0;
}

void try_trigger_smi();

void bench_loopback_test()
{
    unsigned char msg[MSG_SIZE];
    unsigned char recv_msg[PKT_SIZE];
    int recv_len = 0;

checkpoint("Start requesting clock");

    memset(msg, 'A', sizeof(msg));
    put_out_buffer(msg, sizeof(msg));

checkpoint("Trigger SMI");

//    ocall_smi();
    try_trigger_smi();

checkpoint("Return and enter SGX");

    memset(recv_msg, 0, sizeof(recv_msg));
    while (0 == get_in_buffer(recv_msg, &recv_len)) {
        ;//_print_hex("loopback test", recv_msg, MSG_SIZE);
    }
checkpoint("Finish requesting clock");

_print_info();

}

