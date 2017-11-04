#ifndef _RTC_H
#define _RTC_H

#include "types.h"

extern int32_t rtc_open (const uint8_t* fname);
extern int32_t rtc_close(int32_t fd);
extern int32_t rtc_read(int32_t fd, void* buf, int32_t nbytes);
extern int32_t rtc_write(int32_t fd, const void* buf, int32_t nbytes);
extern void rtc_handler_40();
extern void init_rtc();

#endif /* _RTC_H */
