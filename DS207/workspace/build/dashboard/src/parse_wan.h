#ifndef DASH_PARSE_WAN_H
#define DASH_PARSE_WAN_H
#include <stddef.h>

/* Input is the CONTENTS of /proc/net/tcp. Counts ESTABLISHED (st=01) connections whose
 * REMOTE address is LAN (10/8, 172.16/12, 192.168/16, 127/8, 169.254/16) vs public, and
 * lists the public ones: {"public_conns":[{"local":"a.b.c.d:p","remote":"a.b.c.d:p"}],
 * "public_count":N,"lan_count":N}. Hex addresses are decoded BIG-ENDIAN (the DS207 is a
 * big-endian PowerPC: C0A8010B == 192.168.1.11); x86 hosts would need byte reversal.
 * Returns bytes written or -1 on overflow/NULL out. Bad input -> zero-count object. */
int build_wan_json(const char *proc_net_tcp, char *out, size_t outlen);
#endif
