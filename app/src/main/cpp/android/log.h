#ifndef ANDROID_LOG_H
#define ANDROID_LOG_H
#define ANDROID_LOG_INFO 4
#define ANDROID_LOG_ERROR 6
#define ANDROID_LOG_WARN 5
static inline int __android_log_print(int p,const char*t,const char*f,...){(void)p;(void)t;(void)f;return 0;}
#endif
