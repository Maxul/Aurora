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


#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <thread>

# include <unistd.h>
# include <pwd.h>
# define MAX_PATH FILENAME_MAX

#include "sgx_urts.h"
#include "App.h"
#include "Enclave_u.h"

/* Global EID shared by multiple threads */
sgx_enclave_id_t global_eid = 0;

typedef struct _sgx_errlist_t {
    sgx_status_t err;
    const char *msg;
    const char *sug; /* Suggestion */
} sgx_errlist_t;

/* Error code returned by sgx_create_enclave */
static sgx_errlist_t sgx_errlist[] = {
    {
        SGX_ERROR_UNEXPECTED,
        "Unexpected error occurred.",
        NULL
    },
    {
        SGX_ERROR_INVALID_PARAMETER,
        "Invalid parameter.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_MEMORY,
        "Out of memory.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_LOST,
        "Power transition occurred.",
        "Please refer to the sample \"PowerTransition\" for details."
    },
    {
        SGX_ERROR_INVALID_ENCLAVE,
        "Invalid enclave image.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ENCLAVE_ID,
        "Invalid enclave identification.",
        NULL
    },
    {
        SGX_ERROR_INVALID_SIGNATURE,
        "Invalid enclave signature.",
        NULL
    },
    {
        SGX_ERROR_OUT_OF_EPC,
        "Out of EPC memory.",
        NULL
    },
    {
        SGX_ERROR_NO_DEVICE,
        "Invalid SGX device.",
        "Please make sure SGX module is enabled in the BIOS, and install SGX driver afterwards."
    },
    {
        SGX_ERROR_MEMORY_MAP_CONFLICT,
        "Memory map conflicted.",
        NULL
    },
    {
        SGX_ERROR_INVALID_METADATA,
        "Invalid enclave metadata.",
        NULL
    },
    {
        SGX_ERROR_DEVICE_BUSY,
        "SGX device was busy.",
        NULL
    },
    {
        SGX_ERROR_INVALID_VERSION,
        "Enclave version was invalid.",
        NULL
    },
    {
        SGX_ERROR_INVALID_ATTRIBUTE,
        "Enclave was not authorized.",
        NULL
    },
    {
        SGX_ERROR_ENCLAVE_FILE_ACCESS,
        "Can't open enclave file.",
        NULL
    },
};

/* Check error conditions for loading enclave */
void print_error_message(sgx_status_t ret)
{
    size_t idx = 0;
    size_t ttl = sizeof sgx_errlist/sizeof sgx_errlist[0];

    for (idx = 0; idx < ttl; idx++) {
        if(ret == sgx_errlist[idx].err) {
            if(NULL != sgx_errlist[idx].sug)
                printf("Info: %s\n", sgx_errlist[idx].sug);
            printf("Error: %s\n", sgx_errlist[idx].msg);
            break;
        }
    }

    if (idx == ttl)
        printf("Error: Unexpected error occurred.\n");
}

/* Initialize the enclave:
 *   Step 1: try to retrieve the launch token saved by last transaction
 *   Step 2: call sgx_create_enclave to initialize an enclave instance
 *   Step 3: save the launch token if it is updated
 */
int initialize_enclave(void)
{
    char token_path[MAX_PATH] = {'\0'};
    sgx_launch_token_t token = {0};
    sgx_status_t ret = SGX_ERROR_UNEXPECTED;
    int updated = 0;

    /* Step 1: try to retrieve the launch token saved by last transaction
     *         if there is no token, then create a new one.
     */
    /* try to get the token saved in $HOME */
    const char *home_dir = getpwuid(getuid())->pw_dir;

    if (home_dir != NULL &&
        (strlen(home_dir)+strlen("/")+sizeof(TOKEN_FILENAME)+1) <= MAX_PATH) {
        /* compose the token path */
        strncpy(token_path, home_dir, strlen(home_dir));
        strncat(token_path, "/", strlen("/"));
        strncat(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME)+1);
    } else {
        /* if token path is too long or $HOME is NULL */
        strncpy(token_path, TOKEN_FILENAME, sizeof(TOKEN_FILENAME));
    }

    FILE *fp = fopen(token_path, "rb");
    if (fp == NULL && (fp = fopen(token_path, "wb")) == NULL) {
        printf("Warning: Failed to create/open the launch token file \"%s\".\n", token_path);
    }

    if (fp != NULL) {
        /* read the token from saved file */
        size_t read_num = fread(token, 1, sizeof(sgx_launch_token_t), fp);
        if (read_num != 0 && read_num != sizeof(sgx_launch_token_t)) {
            /* if token is invalid, clear the buffer */
            memset(&token, 0x0, sizeof(sgx_launch_token_t));
            printf("Warning: Invalid launch token read from \"%s\".\n", token_path);
        }
    }
    /* Step 2: call sgx_create_enclave to initialize an enclave instance */
    /* Debug Support: set 2nd parameter to 1 */
    ret = sgx_create_enclave(ENCLAVE_FILENAME, SGX_DEBUG_FLAG, &token, &updated, &global_eid, NULL);
    if (ret != SGX_SUCCESS) {
        print_error_message(ret);
        if (fp != NULL) fclose(fp);
        return -1;
    }

    /* Step 3: save the launch token if it is updated */
    if (updated == FALSE || fp == NULL) {
        /* if the token is not updated, or file handler is invalid, do not perform saving */
        if (fp != NULL) fclose(fp);
        return 0;
    }

    /* reopen the file with write capablity */
    fp = freopen(token_path, "wb", fp);
    if (fp == NULL) return 0;
    size_t write_num = fwrite(token, 1, sizeof(sgx_launch_token_t), fp);
    if (write_num != sizeof(sgx_launch_token_t))
        printf("Warning: Failed to save launch token to \"%s\".\n", token_path);
    fclose(fp);
    return 0;
}

/* OCall functions */
void ocall_print_string(const char *str)
{
    /* Proxy/Bridge will check the length and null-terminate
     * the input string to prevent buffer overflow.
     */
    printf("%s", str);
}

#if 0
///
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

#define fail_unless(x) { if (0 != (x)) {;} \
    else {printf("FILE: %s LINE: %d\n", __FILE__, __LINE__); abort();} }
#define fail abort

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
  IP4_ADDR(&sgx_gw, 10,112,30,1);
  IP4_ADDR(&sgx_ipaddr, 10,112,30,251);
  IP4_ADDR(&sgx_netmask, 255,0,0,0);

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

static u8_t dhcp_offer[] = {
0x83, 0xF8, 0xFF, 0x0F, 0x85, 0x5A, 0x01, 0x23, 0x45, 0x67, 0x89, 0x89, 0x08, 0x00, 0x45, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x11, 0x3B, 0xE5, 0xC0, 0xA8, 0x7A, 0x1A, 0xC0, 0xA8, 0x84, 0x1C, 0x04, 0xD2, 0x00, 0x7B, 0x00, 0x6C, 0x5C, 0x23, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE, 0xAE,
};

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

/*
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
*/
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
	myAddr.sin_port = htons (1234);
	myAddr.sin_addr.s_addr = htonl (DATA_LOCAL_HOST);
	printf ("Bind socket.\n");
	if (bind (s, (struct sockaddr *)&myAddr, sizeof myAddr) < 0)
		printf ("Can't bind socket: %s", strerror (errno));

	farAddr.sin_family = AF_INET;
	farAddr.sin_port = htons (8066);	/* The `discard' port */

    memset(cbuf, 'F', sizeof(cbuf));
	farAddr.sin_addr.s_addr = htonl (DATA_SINK_HOST);
	if (sendto (s, cbuf, sizeof cbuf, 0, (struct sockaddr *)&farAddr, sizeof farAddr) >= 0)
		printf ("sendto() succeeded!\n");

	close (s);
}

#define BUFLEN 1500	//Max length of buffer
#define PORT 8066	//The port on which to send data
//#define printf
int udp_server(void)
{
	struct sockaddr_in si_me, si_other;

	int s, i, slen = sizeof(si_other) , recv_len;
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

void lwip_test()
{
  printf( "Initiating test\n" );
#if NO_SYS
  lwip_init();
#else
  tcpip_init(NULL, NULL);
#endif

  etharp_setup();

  //transmitUdp();
  udp_server();

  etharp_teardown();
}

///
#endif

#include <err.h>
#include <sys/io.h>
#define PORT_SMI_CMD 0x00b2

void trigger_smi()
{
    // time to trigger the smi
    if (0 != ioperm(PORT_SMI_CMD, 1, 1))
        err(EXIT_FAILURE, "ioperm");

    outb(0xa0, PORT_SMI_CMD);
}

void ocall_smi()
{
    trigger_smi();
}

unsigned long ocall_tsc()
{
	unsigned int a, d;

	asm volatile("rdtscp" : "=a" (a), "=d" (d) : : "%rbx", "%rcx");
//	printf("tsc = %lu\n", ((unsigned long) a) | (((unsigned long) d) << 32));
//	return ((unsigned long) a) | (((unsigned long) d) << 32);
    return a;
}

unsigned char *uio_ram;
void uio_open();
void uio_close();
void uio_poll();

void ocall_uio_poll()
{
    uio_poll();
}

/* Application entry */
int SGX_CDECL main(int argc, char *argv[])
{
    (void)(argc);
    (void)(argv);

    uio_open();

    //thread monitor(do_probe_incoming_packets);
    //monitor.join();

//    lwip_test();
#if 1
    /* Initialize the enclave */
    if(initialize_enclave() < 0){
        printf("Enter a character before exit ...\n");
        getchar();
        return -1;
    }

    ecall_lwip_network(global_eid, uio_ram);

    /* Destroy the enclave */
    sgx_destroy_enclave(global_eid);
#endif
    uio_close();

    printf("\nInfo: Lwip Enclave successfully returned.\n");
    return 0;
}

