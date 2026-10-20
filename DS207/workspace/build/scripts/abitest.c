#include <stdio.h>
#include <string.h>
typedef struct { unsigned char *dptr; unsigned long dsize; } D;
__attribute__((noinline)) unsigned long sink2(D a, D b){ return a.dsize*1000 + b.dsize; }
__attribute__((noinline)) D makeD(const char*s){ D d; d.dptr=(unsigned char*)s; d.dsize=strlen(s)+1; return d; }
int main(void){
    D k = makeD("notify-daemon");   /* dsize should be 14 */
    D v = makeD("x");               /* dsize should be 2  */
    printf("direct: k.dsize=%lu v.dsize=%lu\n", k.dsize, v.dsize);
    printf("by-value through sink2: %lu (expect 14002)\n", sink2(k, v));
    return 0;
}
