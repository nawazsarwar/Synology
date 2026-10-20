#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
int main(void){
    time_t t = time(NULL);               /* -> __time64 */
    struct tm tm; gmtime_r(&t, &tm);     /* -> __gmtime64_r */
    char buf[64]; strftime(buf,sizeof buf,"%Y-%m-%d %H:%M:%S",&tm);
    struct stat st; stat("/", &st);      /* -> __stat_time64 */
    printf("time=%lld year=%d root_mtime=%lld\n",(long long)t, tm.tm_year+1900,(long long)st.st_mtime);
    return 0;
}
