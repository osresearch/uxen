#ifndef _NICKEL_DHCP_H_
#define _NICKEL_DHCP_H_

void dhcp_input(struct nickel *ni, const uint8_t *pkt, size_t len,
        uint32_t saddr, uint32_t daddr);

#endif
