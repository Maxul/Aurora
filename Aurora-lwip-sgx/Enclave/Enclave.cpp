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

#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>      /* vsnprintf */
#include <string.h>
#include <errno.h>
#include <assert.h>

#include "sgx_tcrypto.h"
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

//#define printf

#include "lwip/udp.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"
#include "lwip/stats.h"
#include "lwip/prot/iana.h"
#include "lwip/sockets.h"
#include "lwip/netif.h"
#include "lwip/etharp.h"
#include "netif/ethernet.h"
#include "lwip/dhcp.h"
#include "lwip/prot/dhcp.h"

#include "lwip/init.h"
#if !NO_SYS
#include "lwip/tcpip.h"
#endif

#define fail_unless(x) /*{ if (0 != (x)) {;} \
    else {printf("FILE: %s LINE: %d\n", __FILE__, __LINE__); abort();} }*/
#define fail /*abort*/

#if !LWIP_STATS || !UDP_STATS || !MEMP_STATS || !ETHARP_STATS
#error "This tests needs UDP-, MEMP- and ETHARP-statistics enabled"
#endif
#if !ETHARP_SUPPORT_STATIC_ENTRIES
#error "This test needs ETHARP_SUPPORT_STATIC_ENTRIES enabled"
#endif

struct netif sgx_netif;
static ip4_addr_t sgx_ipaddr, sgx_netmask, sgx_gw;

/* Helper functions */
static void
etharp_remove_all(void)
{
  int i;
  /* call etharp_tmr often enough to have all entries cleaned */
  for(i = 0; i < 0xff; i++) {
    etharp_tmr();
  }
}

err_t ethernetif_init(struct netif *netif);
static void
default_netif_add(void)
{
#if 1
  IP4_ADDR(&sgx_gw, 10,112,73,1);
  IP4_ADDR(&sgx_ipaddr, 10,112,73,80);
  IP4_ADDR(&sgx_netmask, 255,255,0,0);
#else
  IP4_ADDR(&sgx_gw, 10,203,0,1);
  IP4_ADDR(&sgx_ipaddr, 10,203,0,170);
  IP4_ADDR(&sgx_netmask, 255,255,255,224);
#endif

  fail_unless(netif_default == NULL);
  netif_set_default(netif_add(&sgx_netif, &sgx_ipaddr, &sgx_netmask,
                              &sgx_gw, &sgx_netif, ethernetif_init, ip_input));

  netif_set_up(&sgx_netif);
  //dhcp_start(&sgx_netif);
}

static void
default_netif_remove(void)
{
//  fail_unless(netif_default == &sgx_netif);
  netif_remove(&sgx_netif);
}

/* Setups/teardown functions */
static void
etharp_setup(void)
{
  etharp_remove_all();
  default_netif_add();
}

static void
etharp_teardown(void)
{
  etharp_remove_all();
  default_netif_remove();
}

void send_pkt(struct netif *netif, const u8_t *data, size_t len)
{
  struct pbuf *p, *q;
  LWIP_ASSERT("pkt too big", len <= 0xFFFF);
  p = pbuf_alloc(PBUF_RAW, (u16_t)len, PBUF_POOL);

  if (0) {
    /* Dump data */
    u32_t i;
    printf("RX data (len %d)", p->tot_len);
    for (i = 0; i < len; i++) {
      printf(" %02X", data[i]);
    }
    printf("\n");
  }

  fail_unless(p != NULL);
  for(q = p; q != NULL; q = q->next) {
    memcpy(q->payload, data, q->len);
    data += q->len;
  }
  netif->input(p, netif);
}

#define DATA_SINK_HOST	((10 << 24) | (112 << 16) | (83 << 8) | 194)
#define DATA_LOCAL_HOST	((10 << 24) | (112 << 16) | (30 << 8) | 251)
static void
transmitUdp (void)
{
	int s;
	int i;
	int opt;
	static struct sockaddr_in myAddr, farAddr;
	static char cbuf[200];

	printf ("Create socket.\n");
	s = socket (AF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		printf ("Can't create socket: %s", strerror (errno));
	myAddr.sin_family = AF_INET;
	myAddr.sin_port = htons (5120);
	myAddr.sin_addr.s_addr = htonl (DATA_LOCAL_HOST);
	printf ("Bind socket.\n");
	if (bind (s, (struct sockaddr *)&myAddr, sizeof myAddr) < 0)
		printf ("Can't bind socket: %s", strerror (errno));

	farAddr.sin_family = AF_INET;
	farAddr.sin_port = htons (8066);	/* The `discard' port */

    memset(cbuf, 'A', sizeof(cbuf));
	farAddr.sin_addr.s_addr = htonl (DATA_SINK_HOST);
	if (sendto (s, cbuf, sizeof cbuf, 0, (struct sockaddr *)&farAddr, sizeof farAddr) >= 0)
		printf ("sendto() succeeded!\n");

	close (s);
}

#define SERVER "172.20.10.3"
#define BUFLEN 512	//Max length of buffer
#define PORT 8066	//The port on which to send data

int udp_client(void)
{
	struct sockaddr_in si_other;
	int s, slen=sizeof(si_other);
	char buf[BUFLEN];
	char message[BUFLEN] = " I love you all\n";

	if ( (s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		printf("socket");
		return -1;
	}

	memset((char *) &si_other, 0, sizeof(si_other));
	si_other.sin_family = AF_INET;
	si_other.sin_port = htons(PORT);

	if (inet_aton(SERVER , &si_other.sin_addr) == 0)
	{
		printf("inet_aton() failed\n");
		return -1;
	}

	//while(1)
	//{
		printf("Enter message : ");
		//scanf("%s", message);
		static int i = 0;
		i++;
		snprintf(message, sizeof message, "Love is to give XXX%d\n", i);

		//send the message
		if (sendto(s, message, strlen(message) , 0 , (struct sockaddr *) &si_other, slen) < 0)
		{
			return -1;
		}
		printf("%s\n", buf);
	//}

	close(s);
	return 0;
}

int udp_server(void)
{
	struct sockaddr_in si_me, si_other;

	int s, i;
	socklen_t slen = sizeof(si_other) , recv_len;
	char buf[BUFLEN];

	//create a UDP socket
	if ((s=socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
	{
		printf("socket\n");
	}

	// zero out the structure
	memset((char *) &si_me, 0, sizeof(si_me));

	si_me.sin_family = AF_INET;
	si_me.sin_port = htons(PORT);
	si_me.sin_addr.s_addr = htonl(INADDR_ANY);

	//bind socket to port
	if( bind(s , (struct sockaddr*)&si_me, sizeof(si_me) ) == -1)
	{
		printf("bind\n");
	}

	//keep listening for data
	while (1)
	{
		printf("Waiting for data...\n");
		memset(buf, 0, sizeof buf);

		//try to receive some data, this is a blocking call
		if ((recv_len = recvfrom(s, buf, BUFLEN, 0, (struct sockaddr *) &si_other, &slen)) == -1)
		{
			printf("recvfrom()\n");
		}

		//print details of the client/peer and the data received
		printf("Received packet from %s:%d\n", inet_ntoa(si_other.sin_addr), ntohs(si_other.sin_port));
		printf("Data: %s\n" , buf);

		//now reply the client with the same data
		if (sendto(s, buf, recv_len, 0, (struct sockaddr*) &si_other, slen) == -1)
		{
			printf("sendto()\n");
		}

	}

	close(s);
	return 0;
}

unsigned char *uio_ram = nullptr;

void ecall_lwip_network(unsigned char *ram)
{
  uio_ram = ram;
  printf( "Initiating test: %p\n", uio_ram );
  
#if 1
#if NO_SYS
  lwip_init();
#else
  tcpip_init(NULL, NULL);
#endif

  etharp_setup();

  //transmitUdp();
  //udp_client();
  udp_server();

  etharp_teardown();
#endif
}

void ecall_yes()
{
    printf("yes");
}

void ecall_dummy() {}

#include <ctype.h>
void _print_hex(const char* what, const void* v, const unsigned long l)
{
  const unsigned char* p = (unsigned char*)v;
  unsigned long x, y = 0, z;
  printf("%s contents: \n", what);
  for (x = 0; x < l; ) {
      printf("%02X ", p[x]);
      if (!(++x % 16) || x == l) {
         if((x % 16) != 0) {
            z = 16 - (x % 16);
            if(z >= 8)
               printf(" ");
            for (; z != 0; --z) {
               printf("   ");
            }
         }
         printf(" | ");
         for(; y < x; y++) {
            if((y % 8) == 0)
               printf(" ");
            if(isgraph(p[y]))
               printf("%c", p[y]);
            else
               printf(".");
         }
         printf("\n");
      }
      else if((x % 8) == 0) {
         printf(" ");
      }
  }
}

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

#define N	1000
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

void checkpoint(const char *what)
{
    long unsigned tsc = 0;
    ocall_tsc(&tsc);
    printf("%s : TSC = %lu\n", what, tsc);
}

void bench_clock_service(unsigned char *ram)
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

checkpoint("start to request time");

    memset(key, 0xca, crypto_aead_aes256gcm_KEYBYTES);
    memset(nonce, 0xff, crypto_aead_aes256gcm_NPUBBYTES);
    memset(message, 'A', message_len);

checkpoint("start to encrypt in EPC");

    crypto_aead_aes256gcm_encrypt(ciphertext, &found_ciphertext_len,
                                      message, message_len,
                                      ad, ad_len, NULL, nonce, key);

//    _print_hex("cipher from SGX", ciphertext, found_ciphertext_len);
checkpoint("encrypt done");

    unsigned char *addr = ram + (0x64);
    memcpy(addr, ciphertext, found_ciphertext_len);
//    int result = memcmp(addr, ciphertext, found_ciphertext_len);
//    printf("memcmp ret = %d\n", result);
checkpoint("copy to channel done");

checkpoint("trigger SMI");
printf("\n");
    ocall_smi();

checkpoint("return and enter SGX");

    memcpy(ciphertext, addr, ciphertext_len);
//    _print_hex("cipher from SMM", ciphertext, ciphertext_len);
checkpoint("copy to EPC done");

    found_message_len = 1;
    if (crypto_aead_aes256gcm_decrypt(decrypted, &found_message_len,
                                          NULL, ciphertext, ciphertext_len,
                                          ad, ad_len, nonce, key) != 0) {
        printf("Verification of test vector #%u failed\n", (unsigned int) i);
    }

checkpoint("EPC decrypt done");

//    _print_hex("msg from SMM", decrypted, found_message_len);
    sodium_free(message);
    sodium_free(ad);
    sodium_free(ciphertext);
    sodium_free(decrypted);

    sodium_free(key);
    sodium_free(mac);
    sodium_free(nonce);
}

void init_metadata();
int put_out_buffer(unsigned char *message, const int message_len);
int get_in_buffer(unsigned char *message, int *message_len);
void loopback_test();

void ecall_safeio(unsigned char *uio_mem)
{
//    init_metadata();
//    _print_hex("", uio_mem, 256);
#if 0
int x = 1000;
while (x--) {
static int n = 1;
printf("Microbenchmark inside SGX : %d\n", n++);
//    loopback_test();
    bench_clock_service(uio_mem);
printf("\n");
}
#endif
}

