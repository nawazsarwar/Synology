#include <stdio.h>
#include <sys/utsname.h>
int main(void){
    struct utsname u; uname(&u);
    printf("cross PPC musl static OK | kernel=%s machine=%s\n", u.release, u.machine);
    return 0;
}
