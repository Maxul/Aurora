/**
    This is file is ported from ipxe/src/drivers/net/pcnet32.c
    Please refer to Am79C970A spec.
*/

#include "output.h" // dprintf
#include "string.h" // memcpy
#include <errno.h>

#include "smx.h"

// PCnet/PCI II 79C970A
#include "ipxe/pcnet32.h"

/**
    Since the native memset provided by SeaBIOS is so inefficient in 32-bit SMM
    therefore we need to replace it for better performance
*/
#define memset __builtin_memset

extern void *pcnet32_ioaddr;
static void *ioaddr = NULL;

#define dprintf

static struct pcnet32_init_block *p_init_block;
static struct pcnet32_access *a = &pcnet32_wio;

static u8 hw_addr[6];
static u8 tx_frame[1600];

/* This is for XMT a frame in SMM mode only */
static int __CSR5 = 0;
static short __XMTRC = 0;

static inline void pcnet32_backdoor_step1(void) {
  // save context
  __CSR5 = a->read_csr(ioaddr, 5);
  //    dprintf(1, "Enter Suspend Mode\n" );
  a->write_csr(ioaddr, 5, 0x0001);

  __XMTRC = a->read_csr(ioaddr, 74);
  //    dprintf(1, "XMTRC : %d\n", __XMTRC );
  a->write_csr(ioaddr, 5, __CSR5);
  //    dprintf(1, "Leave Suspend Mode\n" );
}

static inline void pcnet32_backdoor_step2(void) {
  // restore context
  __CSR5 = a->read_csr(ioaddr, 5);
  //    dprintf(1, "Enter Suspend Mode\n" );
  a->write_csr(ioaddr, 74, __XMTRC);
  a->write_csr(ioaddr, 5, __CSR5);
  //    dprintf(1, "Leave Suspend Mode\n" );
}
/* XMT a frame done */

static inline void do_pcnet32_transmit(u8 *data, int len) {
  int i;
//  dprintf(1, "pcnet32_transmit %d\n", len);
//  _print_hex("", data, len);
//dprintf(1, "data : %p tx_frame : %p\n", data, tx_frame);

  uint32_t tx_len = (len < 64) ? 64 : len;
  struct pcnet32_tx_desc *tx_curr_desc = p_init_block->tx_ring;
  tx_curr_desc += TX_RING_SIZE - __XMTRC;

  /* Configure current descriptor to transmit packet */
  tx_curr_desc->length = cpu_to_le16(-tx_len);
  tx_curr_desc->misc = 0x00000000;
  tx_curr_desc->base = cpu_to_le32(data);

  /* Owner changes after the other status fields are set */
  wmb();
  tx_curr_desc->status = cpu_to_le16(DescOwn | StartOfPacket | EndOfPacket);

  /* Trigger an immediate send poll */
  a->write_csr(ioaddr, 0, IntEnable | TxDemand);

  // clear plain-text frame in kernel memory
  memset(tx_frame, 0, sizeof(tx_frame));

}

void pcnet32_probe() {
  if (pcnet32_ioaddr)
    ioaddr = pcnet32_ioaddr;
  else {
    dprintf(1, "NO IO for PCNET32");
    return;
  }
  int i;
  int val;
  int pcnet32_init = 0;

#if 1
  int chip_version;

  chip_version = a->read_csr(ioaddr, 88) | (a->read_csr(ioaddr, 89) << 16);
  chip_version = (chip_version >> 12) & 0xffff;
  dprintf(1, "PCnet chip version is 0x%x\n", chip_version);

  /* In most chips, after a chip reset, the ethernet address is read from
   * the station address PROM at the base address and programmed into the
   * "Physical Address Registers" CSR12-14.
   * As a precautionary measure, we read the PROM values and complain if
   * they disagree with the CSRs.  If they miscompare, and the PROM addr
   * is valid, then the PROM addr is used.
   */
  for (i = 0; i < 3; i++) {
    unsigned int val;
    val = pcnet32_wio_read_csr(ioaddr, i + 12) & 0x0ffff;
    hw_addr[2 * i] = val & 0x0ff;
    hw_addr[2 * i + 1] = (val >> 8) & 0x0ff;
  }
#endif

  pcnet32_init = (a->read_csr(ioaddr, 2) << 16) | (a->read_csr(ioaddr, 1));
  p_init_block = (struct pcnet32_init_block *)pcnet32_init;

#if 0
    u16 tlen_rlen = p_init_block->tlen_rlen;
    dprintf(1, "\ntlen %d rlen %d\n", 2<<(tlen_rlen>>12), 2<<((tlen_rlen&0xff)>>4));
    for ( i = 0; i < 6; i++ ) {
//		dprintf(1, "%x:", p_init_block->phys_addr[i]);
    }
#endif
  /* Switch pcnet32 to 32bit mode */
  a->write_bcr(ioaddr, 20, PCNET32_SWSTYLE_PCNET32);

  /* Enable Auto-Pad, disable interrupts */
  a->write_csr(ioaddr, 4, 0x0915);

  /* Enable TINT and RINT masks */
  val = a->read_csr(ioaddr, 3);
  val &= ~(RxIntMask | TxIntMask);
  a->write_csr(ioaddr, 3, val);

  /* Enable interrupts */
  a->write_csr(ioaddr, 0, IntEnable);
}

static inline void pcnet32_tx_enclave_packet() {
  int tmp_len = 0;
  char smm_tmp[MSG_SIZE];
  
  // get the packet from the shared memory
  while (0 == get_out_buffer(smm_tmp, &tmp_len)) {
    struct MAC_Prot *kp = (struct MAC_Prot *)smm_tmp;
    unsigned char *frame = smm_tmp + sizeof(kp->ulen);
    // assign the mac address
//    str2mac("FF:FF:FF:FF:FF:FF", frame);
    // this is the MAC address of next hop. can be ontained using rx introspection
    str2mac("f4:8e:38:f2:fa:a0", frame);
    memcpy(frame + 6, hw_addr, sizeof hw_addr);
    
    memcpy(tx_frame , frame, kp->ulen);
    
    do_pcnet32_transmit(tx_frame, kp->ulen);
  }
}

static inline void pcnet32_dispatch_rx_packets(struct pcnet32_rx_desc *rxs) {
    static int n = 1;
    u8 smm_tmp_buf[1600];

  // check if packet header is longer
  struct ip *ip = (struct ip *)(rxs->base + 14);
  //    dprintf(1, "ip->ip_vhl %d\n", ip->ip_vhl);
  if (70 != ip->ip_vhl) {
    return;
  }

//_print_hex("ether frame from NIC", rxs->base, rxs->msg_length);

  // search the IP_OPTIONS field to find corresponding packet
  unsigned char *pPacket = (unsigned char *)(rxs->base + 34);

  // currently this is a demo that shows how we identify the special packet for enclaves
  static const char _IP_OPTIONS_FLAG[4] = {0x0, 0x0, 0x0, 0x0};

  if (0 == memcmp(pPacket, _IP_OPTIONS_FLAG, 4)) {
    dprintf(1, "IN ");

    struct MAC_Prot *cp = (struct MAC_Prot *)smm_tmp_buf;
    // drop the ethernet header
    cp->ulen = rxs->msg_length - sizeof(struct ether_header);
//    dprintf(1, "ip packet length %d\n", cp->ulen);
    // copy the ip packet only
    memcpy(cp->umsg, rxs->base + sizeof(struct ether_header), cp->ulen);
    put_in_buffer(smm_tmp_buf, /*cp->ulen + sizeof(cp->ulen)*/(1<<12)-64);
    /* Clear the descriptor */
    memset(rxs, 0, sizeof(*rxs));

    // notify the uio driver inside
    trigger_self_ipi(0x3a);
  }
}

static inline void pcnet32_process_rx_packets() {
  int i, rx_cnt;

  struct pcnet32_rx_desc *rxs = p_init_block->rx_ring;

  rx_cnt = 0;
  for (i = 0; i < RX_RING_SIZE; i++) {
    if (0x340 == ((rxs->status) & 0xffff)) {
#ifdef PRINT
      dprintf(
          1, "RX %d\tbase 0x%x\tbuf_length %d\tstatus 0x%x\tmsg_length %d\n", i,
          rxs->base, -rxs->buf_length, (rxs->status) & 0xffff, rxs->msg_length);
#endif
      pcnet32_dispatch_rx_packets(rxs);
      rx_cnt++;
    }
    rxs++;
  }
  if (rx_cnt > 1)
    dprintf(1, "pcnet32_process_rx_packets %d\n", rx_cnt);
}

static inline void pcnet32_process_tx_packets() {
  int i, tx_cnt;

  struct pcnet32_tx_desc *txs = p_init_block->tx_ring;

  tx_cnt = 0;
  for (i = 0; i < TX_RING_SIZE; i++) {

    if (txs->base) {
#ifdef PRINT
      dprintf(1, "TX %d\tbase 0x%x\tlength %d\tstatus 0x%x\tmisc 0x%x\n", i,
              txs->base, -txs->length, (txs->status) & 0xffff, txs->misc);
#endif
      tx_cnt++;
    }
    txs++;
  }
  if (tx_cnt > 1)
    dprintf(1, "pcnet32_process_tx_packets %d\n", tx_cnt);
}

void pcnet32_handler() {
  static int rx = 0, tx = 0;

  int Int = a->read_csr(ioaddr, 0);

  if (Int & TxInt) {
        dprintf(1, "TxInt %d\n", tx++ );pcnet32_backdoor_step1();pcnet32_backdoor_step2();
        //pcnet32_process_tx_packets();
    trigger_self_ipi(0x3b);
  }
  if (Int & RxInt) {
        dprintf(1, "RxInt %d\n", rx++ );
        pcnet32_process_rx_packets();
    trigger_self_ipi(0x3b);
  }
  if (!(Int & TxInt) && !(Int & RxInt)) {
        dprintf(1, "OUT\n");
        // figure out which is the next available Transmit Descriptor Ring Location
        pcnet32_backdoor_step1();
//        pcnet32_tx_enclave_packet();
        pcnet32_backdoor_step2();
  }
}

#undef memset

