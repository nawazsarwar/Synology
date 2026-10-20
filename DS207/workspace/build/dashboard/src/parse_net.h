#ifndef DASH_PARSE_NET_H
#define DASH_PARSE_NET_H
#include <stddef.h>

/* Input is the CONTENTS of /proc/net/dev. Emits JSON array of
 * {iface, rx_bytes, tx_bytes} for every interface except "lo" (cumulative counters).
 * Returns bytes written (excluding NUL) or -1 on overflow/NULL args. Bad input -> "[]". */
int build_net_json(const char *proc_net_dev, char *out, size_t outlen);
#endif
