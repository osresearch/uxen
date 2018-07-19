/* Copyright (c) 2007, XenSource Inc.
 * All rights reserved.
 *
 * XenSource proprietary code.
 */

#ifndef _RELATIVE_PATH_H_
#define _RELATIVE_PATH_H_

// #include <syslog.h>

#define MAX_NAME_LEN 1000

/*
 * returns a relative path from @src to @dest
 * result should be freed
 */
char *relative_path_to(char *src, char *dest, int *err);

#endif
