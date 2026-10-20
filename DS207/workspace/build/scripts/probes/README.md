# Debug probes

Tiny throwaway C programs written during the Samba-on-2.6.24 debugging to isolate
specific kernel-ABI / codegen bugs. Kept for the record; not part of the build.

| Probe | What it isolated |
|---|---|
| `timetest.c` / `t64.c` | the broken `time()` / `clock_gettime` (musl time64 vs 2.6.24) that broke NTLMSSP auth |
| `socktest.c` | `SO_REUSEPORT` ENOPROTOOPT on kernel 2.6.24 |
| `epolltest.c` | epoll/eventfd 2.6.27-syscall fallbacks |
| `vlatest.c` | the PowerPC VLA / compound-literal stack-corruption codegen bug |
| `abitest.c`, `dyn.c`, `lib.c`, `m.c`, `tc_test.c` | assorted toolchain/ABI sanity checks |

The real compatibility shim that resulted is `../syscompat.c`.
