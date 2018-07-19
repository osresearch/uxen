
#ifndef _UXENEVENT_H_
#define _UXENEVENT_H_

#include "../common/debug-user.h"

#define JPWerr(a, fmt, ...)  do { uxen_err(fmt, ##__VA_ARGS__ ); Sleep(20000); uxen_err(fmt, ## __VA_ARGS__ ); exit(a); } while (0)

#endif  /* _UXENEVENT_H_ */
