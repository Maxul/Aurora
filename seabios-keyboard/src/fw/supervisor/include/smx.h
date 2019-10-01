#ifndef __SMX_H__
#define __SMX_H__

#define SHM_SIZE  (1 << 20)
#define PKT_SIZE  (1 << 12)
#define MSG_SIZE  (PKT_SIZE - 16)

#include <stdint.h>

// Memory oundary checking
int is_within_smram(const void *addr, int size);

// shared memory buffer libtool
void init_ring_buffer(void);
int get_out_buffer(unsigned char *message, int *message_len);
int put_in_buffer(const unsigned char *raw_msg, const int msg_len);

// debug tool
void checkpoint(const char *what);
void _print_info(void);
void loopback(void);

// Intel rdrand instructions

int get_random_number_8(uint8_t *rand);
int get_random_number_32(uint32_t *rand);
int get_random_number_64(uint64_t *rand);

/*
 *
 * Ethernet protocol
 *
 */

#define ETH_ALEN		6	/* Size of Ethernet address */
#define ETH_HLEN		14	/* Size of ethernet header */
#define	ETH_ZLEN		60	/* Minimum packet */
#define	ETH_FRAME_LEN		1514	/* Maximum packet */
#define ETH_DATA_ALIGN		2	/* Amount needed to align the data after an ethernet header */
#define	ETH_MAX_MTU		(ETH_FRAME_LEN-ETH_HLEN)

/**
 * Check if Ethernet address is all zeroes
 *
 * @v addr		Ethernet address
 * @ret is_zero		Address is all zeroes
 */
static inline int is_zero_ether_addr ( const void *addr ) {
	const uint8_t *addr_bytes = addr;

	return ( ! ( addr_bytes[0] | addr_bytes[1] | addr_bytes[2] |
		     addr_bytes[3] | addr_bytes[4] | addr_bytes[5] ) );
}

/**
 * Check if Ethernet address is a multicast address
 *
 * @v addr		Ethernet address
 * @ret is_mcast	Address is a multicast address
 *
 * Note that the broadcast address is also a multicast address.
 */
static inline int is_multicast_ether_addr ( const void *addr ) {
	const uint8_t *addr_bytes = addr;

	return ( addr_bytes[0] & 0x01 );
}

/**
 * Check if Ethernet address is locally assigned
 *
 * @v addr		Ethernet address
 * @ret is_local	Address is locally assigned
 */
static inline int is_local_ether_addr ( const void *addr ) {
	const uint8_t *addr_bytes = addr;

	return ( addr_bytes[0] & 0x02 );
}

/**
 * Check if Ethernet address is the broadcast address
 *
 * @v addr		Ethernet address
 * @ret is_bcast	Address is the broadcast address
 */
static inline int is_broadcast_ether_addr ( const void *addr ) {
	const uint8_t *addr_bytes = addr;

	return ( ( addr_bytes[0] & addr_bytes[1] & addr_bytes[2] &
		   addr_bytes[3] & addr_bytes[4] & addr_bytes[5] ) == 0xff );
}

/**
 * Check if Ethernet address is valid
 *
 * @v addr		Ethernet address
 * @ret is_valid	Address is valid
 *
 * Check that the Ethernet address (MAC) is not 00:00:00:00:00:00, is
 * not a multicast address, and is not ff:ff:ff:ff:ff:ff.
 */
static inline int is_valid_ether_addr ( const void *addr ) {
	return ( ( ! is_multicast_ether_addr ( addr ) ) &&
		 ( ! is_zero_ether_addr ( addr ) ) );
}

/** Ethernet broadcast MAC address */
uint8_t eth_broadcast[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

/**
 * Generate random Ethernet address
 *
 * @v hw_addr		Generated hardware address
 */
void eth_random_addr ( void *hw_addr ) {
	uint8_t *addr = hw_addr;
	unsigned int i;

	for ( i = 0 ; i < ETH_ALEN ; i++ )
		get_random_number_8 ( addr[i] );
	addr[0] &= ~0x01; /* Clear multicast bit */
	addr[0] |= 0x02; /* Set locally-assigned bit */
}

/**
 * Transcribe Ethernet address
 *
 * @v ll_addr		Link-layer address
 * @ret string		Link-layer address in human-readable format
 */
const char * eth_ntoa ( const void *ll_addr ) {
	static char buf[18]; /* "00:00:00:00:00:00" */
	const uint8_t *eth_addr = ll_addr;

	snprintf ( buf, sizeof buf, "%0x:%0x:%0x:%0x:%0x:%0x",
		  eth_addr[0], eth_addr[1], eth_addr[2],
		  eth_addr[3], eth_addr[4], eth_addr[5] );
	return buf;
}

/*
    TCP/IP protocol
*/
#define __packed __attribute__((packed))
#define __aligned(x) __attribute__((aligned(x)))

#define	ETHER_ADDR_LEN		6	/* length of an Ethernet address */
#define	ETHERTYPE_IP		0x0800	/* IP protocol */
#define	IPPROTO_TCP		6		/* tcp */
#define	IPPROTO_UDP	17	/* user datagram protocol */

#define	ti_next		ti_i.ih_next
#define	ti_prev		ti_i.ih_prev
#define	ti_x1		ti_i.ih_x1
#define	ti_pr		ti_i.ih_pr
#define	ti_len		ti_i.ih_len
#define	ti_src		ti_i.ih_src
#define	ti_dst		ti_i.ih_dst
#define	ti_sport	ti_t.th_sport
#define	ti_dport	ti_t.th_dport
#define	ti_seq		ti_t.th_seq
#define	ti_ack		ti_t.th_ack
#define	ti_x2		ti_t.th_x2
#define	ti_off		ti_t.th_off
#define	ti_flags	ti_t.th_flags
#define	ti_win		ti_t.th_win
#define	ti_sum		ti_t.th_sum
#define	ti_urp	ti_t.th_urp

typedef	unsigned char	u_char;
typedef	unsigned short	u_short;
typedef	unsigned int	u_int;
typedef	unsigned long	u_long;

typedef	u32 tcp_seq;
typedef u32 in_addr_t;
typedef	char *caddr_t;
struct in_addr { in_addr_t s_addr; };

struct __attribute__((__packed__)) ether_header {
	unsigned char	ether_dhost[ETHER_ADDR_LEN];
	unsigned char	ether_shost[ETHER_ADDR_LEN];
	unsigned short	ether_type;
} __packed __aligned(1);

struct __attribute__((__packed__)) ip {
	u_char	ip_vhl;		/* header length */ /* version */
	u_char	ip_tos;			/* type of service */
	u_short	ip_len;			/* total length */
	u_short	ip_id;			/* identification */
	u_short	ip_off;			/* fragment offset field */
	u_char	ip_ttl;			/* time to live */
	u_char	ip_p;			/* protocol */
	u_short	ip_sum;			/* checksum */
	struct	in_addr ip_src,ip_dst;	/* source and dest address */
} __packed __aligned(1);

struct __attribute__((__packed__)) tcphdr {
	u_short	th_sport;		/* source port */
	u_short	th_dport;		/* destination port */
	tcp_seq	th_seq;			/* sequence number */
	tcp_seq	th_ack;			/* acknowledgement number */
	u_char	th_x2_off;		/* (unused) */ /* data offset */
	u_char	th_flags;
	u_short	th_win;			/* window */
	u_short	th_sum;			/* checksum */
	u_short	th_urp;			/* urgent pointer */
} __packed __aligned(1);

struct __attribute__((__packed__)) ipovly {
	caddr_t	ih_next;
	caddr_t ih_prev;		/* for protocol sequence q's */
	u_char	ih_x1;			/* (unused) */
	u_char	ih_pr;			/* protocol */
	u_short	ih_len;			/* protocol length */
	struct	in_addr ih_src;		/* source internet address */
	struct	in_addr ih_dst;		/* destination internet address */
} __packed __aligned(1);

struct __attribute__((__packed__)) tcpiphdr {
	struct	ipovly ti_i;		/* overlaid ip structure */
	struct	tcphdr ti_t;		/* tcp header */
} __packed __aligned(1);

struct __attribute__((__packed__)) udphdr {
	u_short	uh_sport;		/* source port */
	u_short	uh_dport;		/* destination port */
	u_short	uh_ulen;		/* udp length */
	u_short	uh_sum;			/* udp checksum */
} __packed __aligned(1);

/*
    IO APIC utilities
*/
u32 ioapic_read_reg(unsigned reg);
void ioapic_write_reg(unsigned reg, u32 value);

// MAC/IP address print
void stack_main(const u8 *pBuffer);

static inline void str2mac(const char *str, unsigned char *mac)
{
	unsigned char c, i = 0;
	int acc = 0;

	do {

		c = str[i];
		if ((c >= '0') && (c <= '9')) {
			acc *= 16;
			acc += (c - '0');
		} else if ((c >= 'a') && (c <= 'f')) {
			acc *= 16;
			acc += ((c - 'a') + 10) ;
		} else if ((c >= 'A') && (c <= 'F')) {
			acc *= 16;
			acc += ((c - 'A') + 10) ;
		} else {
			*mac++ = acc;
			acc = 0;
		}

		i++;
	} while (c != '\0');
}

static inline void str2ip(const char *str, unsigned char *ip)
{
	unsigned char c, i = 0;
	int acc = 0;

	do {
		c = str[i];
		if ((c >= '0') && (c <= '9')) {
			acc *= 10;
			acc += (c - '0');
		} else {
			*ip++ = acc;
			acc = 0;
		}
		i++;
	} while (c != '\0');
}

/*
    customised protocol packet
*/
struct MAC_Prot {
    unsigned int ulen;
    unsigned char umsg[];
} __packed __aligned(1);

void enclave_request(void);

// pcnet32 helper functions

//void pcnet32_probe (  );
void pcnet32_ring_buffer();

// stack heap
void *sgx_malloc(size_t size);
void *sgx_calloc(size_t num, size_t size);
void *sgx_realloc(void *packet, size_t size);
void sgx_free(void *packet);

// string

int memset_s(void *s, size_t smax, int c, size_t n);

// memory barrier
#define mb() __asm__ __volatile__ ("" : : : "memory")
#define rmb()	mb()
#define wmb()   mb()

// customised heap
#define free(x) sgx_free(x)
#define malloc(x) sgx_malloc(x)
#define realloc(x, y) sgx_realloc(x, y)

#endif /* __SMX_H__ */
