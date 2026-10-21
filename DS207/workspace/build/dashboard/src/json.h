#ifndef DASH_JSON_H
#define DASH_JSON_H
#include <stddef.h>

typedef struct { char *buf; size_t len, cap; } jb;

void jb_init(jb *b);
void jb_free(jb *b);
void jb_raw(jb *b, const char *s);                       /* append literal */
void jb_kstr(jb *b, const char *key, const char *val);   /* "key":"escaped" */
void jb_knum(jb *b, const char *key, long long val);
void jb_kreal(jb *b, const char *key, double val);

#endif
