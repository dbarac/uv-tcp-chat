#include <stdio.h>
#include <uv.h>

#define MAC_ADDR_LEN 6
#define IP4_ADDR_LEN 4
#define IP_STR_LEN 16


void extract_mac(unsigned char packet_bytes[]) {
  printf("Destination MAC address: ");
  for (int i = 0; i < MAC_ADDR_LEN - 1; i++) {
    printf("%02x:", packet_bytes[i]);
  }
  printf("%02x\n", packet_bytes[MAC_ADDR_LEN - 1]);

  int source_start = MAC_ADDR_LEN;
  int source_end = 2 * MAC_ADDR_LEN;
  printf("Source MAC address: ");
  for (int i = source_start; i < source_end - 1; i++) {
    printf("%02x:", packet_bytes[i]);
  }
  printf("%02x\n", packet_bytes[source_end - 1]);
}

void extract_ip(unsigned char packet_bytes[]) {
  int ethertype_offset = 2 * MAC_ADDR_LEN;
  int payload_offset = 14;
  // check for optional vlan tag
  if ((unsigned) packet_bytes[ethertype_offset] == 0x8100) {
    payload_offset += 4;
  }

  char source_ip[IP_STR_LEN];
  int ip_source_offset = payload_offset + 12;
  uv_inet_ntop(AF_INET, &packet_bytes[ip_source_offset], source_ip, IP_STR_LEN);
  printf("Source ip: %s\n", source_ip);

  char dest_ip[IP_STR_LEN];
  int ip_dest_offset = ip_source_offset + IP4_ADDR_LEN;
  uv_inet_ntop(AF_INET, &packet_bytes[ip_dest_offset], dest_ip, IP_STR_LEN);
  printf("Destination ip: %s\n", dest_ip);
}

int main() {
  char packet_bytes[] = {
    0x00, 0x13, 0x3b, 0x0c, 0x80, 0x0b, 0x00, 0x90,
    0x4c, 0x2c, 0x30, 0x02, 0x08, 0x00, 0x45, 0x00,
    0x00, 0x51, 0xdc, 0xd6, 0x40, 0x00, 0x40, 0x06,
    0xd9, 0xc4, 0xc0, 0xa8, 0x01, 0x01, 0xc0, 0xa8,
    0x01, 0xba, 0x13, 0xae, 0xc2, 0xca, 0x7e, 0x02,
    0xe0, 0xd3, 0xff, 0x3a, 0x15, 0x07, 0x80, 0x18,
    0x01, 0xc5, 0x38, 0x89, 0x00, 0x00, 0x01, 0x01,
    0x08, 0x0a, 0x00, 0x00, 0x3c, 0x05, 0x3e, 0x7c,
    0x34, 0xed, 0x41, 0x73, 0x74, 0x65, 0x72, 0x69,
    0x73, 0x6b, 0x20, 0x43, 0x61, 0x6c, 0x6c, 0x20,
    0x4d, 0x61, 0x6e, 0x61, 0x67, 0x65, 0x72, 0x2f,
    0x35, 0x2e, 0x30, 0x2e, 0x31, 0x0d, 0x0a
  };
  extract_mac(packet_bytes);
  extract_ip(packet_bytes);
}
