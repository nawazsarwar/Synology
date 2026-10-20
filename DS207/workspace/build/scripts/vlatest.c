#include <stdio.h>
#include <string.h>
typedef struct { const char *p; unsigned long n; } D;
__attribute__((noinline)) void fill(char *b, int n){ for (int i=0;i<n;i++) b[i] = "8503"[i%4]; }
__attribute__((noinline)) unsigned long test(int len){
    D key; key.p = "notify-daemon"; key.n = 14;
    char vla[len];                 /* VLA */
    fill(vla, len);
    return key.n;                  /* must still be 14 */
}
int main(void){
    unsigned long r = test(20);
    printf("key.n after VLA = %lu (expect 14)%s\n", r, r==14 ? " OK" : "  <-- CORRUPTED");
    return 0;
}
