#include "output.h" // dprintf
#include "string.h" // memcpy
#include "x86.h" // inb

#include "sodium.h"
#include "microbench.h"
#include "smx.h"

/**
    Since the native memset provided by SeaBIOS is so inefficient in 32-bit SMM
    therefore we need to replace it for better performance
*/
#define memset __builtin_memset

/**
    In/Out Ring Buffer ownership
*/

#define IN_SMM  0x00
#define IN_SGX  0x01
#define OUT_SMM 0x02
#define OUT_SGX 0x03

// Currently we only set 64 entries for one buffer
#define MAX_ENTRY (64)

void *shmem_base = NULL;

struct metadata {
    char in_bitmap[MAX_ENTRY];
    char out_bitmap[MAX_ENTRY];
    char reserved[PKT_SIZE - 2*MAX_ENTRY];
};

void init_ring_buffer(void)
{
    // obtain the base address from the low-mem
    u64 addr = *(u64 *)0x0064;
    dprintf(1, "shared memory address is %p\n", (void *)addr);

    shmem_base = (void *)addr;
    memset(shmem_base, 0x0, SHM_SIZE);

    /*
        init the metadata block:
        1. all IN (SMM->SGX) belong to SMM
        2. all OUT(SGX->SMM) belong to SGX
    */
    struct metadata *m = (struct metadata *)shmem_base;
    memset(m->in_bitmap, IN_SMM, MAX_ENTRY);
    memset(m->out_bitmap, OUT_SGX, MAX_ENTRY);
}

/**
    This method will encrypt the raw message inside SMRAM,
    search for available IN buffer and put it there.
    The in_bit will be marked at the end.
*/
int put_in_buffer(const unsigned char *raw_msg, const int msg_len)
{
    unsigned char      message[MSG_SIZE];
    unsigned char      ciphertext[PKT_SIZE];
    unsigned char      *ad = (unsigned char *)"";
    unsigned char      nonce[crypto_aead_aes256gcm_NPUBBYTES];
    unsigned char      key[crypto_aead_aes256gcm_KEYBYTES];
    size_t             ad_len = 0;
    unsigned long long found_ciphertext_len;

    int i;
    void *in_pos;
    struct metadata *p_meta = (struct metadata *)shmem_base;
#if 0
    // the message must come from SMRAM
    if (0 == is_within_smram(raw_msg, msg_len)) {
        dprintf(1, "put_in_buffer : %p is not within SMRAM\n", raw_msg);
        return -1;
    }
#endif
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

    // here we use 0xca pattern as a temporary key
    // the key will be obtained in remote attestation phase
    memset(key, 0xca, crypto_aead_aes256gcm_KEYBYTES);
    memset(nonce, 0xff, crypto_aead_aes256gcm_NPUBBYTES);

    crypto_aead_aes256gcm_encrypt(ciphertext, &found_ciphertext_len,
                                      message, MSG_SIZE,
                                      ad, ad_len, NULL, nonce, key);

    // we then copy the encrypted message into the IN buffer
    for (i = 0; i < MAX_ENTRY; ++i) {
        if (IN_SMM == p_meta->in_bitmap[i])
            break;
    }

    if (MAX_ENTRY <= i) {
        return -1;
    }

    // skip the metadata block
    in_pos = shmem_base + sizeof(struct metadata);
    in_pos += (MAX_ENTRY + i) * (PKT_SIZE);
    memcpy(in_pos, ciphertext, PKT_SIZE);

    // turn the ownership bit at the end
    p_meta->in_bitmap[i] = IN_SGX;

    return 0;
}

/**
    This method will get an encrypted message via untrusted memory from SGX,
    copy it into SMRAM and then decrypt it,
    finally stores the readily raw message pointer in the parameter.
    It allows for multiple messages to accumulate to a user-configured size
    and trigger one SMI to process them at one time.
    thus reducing overhead due to expensive context switches.

    Note: the caller should provide enough size in case of overflow!!!
*/
int get_out_buffer(unsigned char *message, int *message_len)
{
    unsigned char      ciphertext[PKT_SIZE];
    unsigned char      *ad = (unsigned char *)"";
    unsigned char      nonce[crypto_aead_aes256gcm_NPUBBYTES];
    unsigned char      key[crypto_aead_aes256gcm_KEYBYTES];
    size_t             ad_len = 0;
    unsigned long long found_message_len;

    *message_len = 0;
    int i;
    void *out_pos;
    struct metadata *p_meta = (struct metadata *)shmem_base;

    // the plaintext must reside within SMRAM
    if (0 == is_within_smram(message, MSG_SIZE)) {
        dprintf(1, "get_out_buffer : %p is not within SMRAM\n", message);
        return -1;
    }

    // first we search for messages to be proccessed
    for (i = 0; i < MAX_ENTRY; ++i) {
        if (OUT_SMM == p_meta->out_bitmap[i])
            break;
    }

    if (MAX_ENTRY <= i) {
        return -1;
    }

    // skip the metadata block
    out_pos = shmem_base + sizeof(struct metadata);
    out_pos += i * (PKT_SIZE);

    // copy the encrypted message from the OUT buffer
    memcpy(ciphertext, out_pos, PKT_SIZE);

    // here we use 0xca pattern as a temporary key
    // the key will be obtained in remote attestation phase
    memset(key, 0xca, crypto_aead_aes256gcm_KEYBYTES);
    memset(nonce, 0xff, crypto_aead_aes256gcm_NPUBBYTES);

    found_message_len = 1;
    if (crypto_aead_aes256gcm_decrypt(message, &found_message_len,
                                          NULL, ciphertext, PKT_SIZE,
                                          ad, ad_len, nonce, key) != 0) {
        dprintf(1, "Verification of test vector #%u failed\n", (unsigned int) i);
        return -1;
    }

    *message_len = found_message_len;

    // turn the ownership bit at the end
    p_meta->out_bitmap[i] = OUT_SGX;

    return 0;
}

#undef memset

