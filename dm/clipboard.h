#ifndef _CLIPBOARD_H_
#define _CLIPBOARD_H_

int clip_service_start(void);
void clip_service_stop(void);
int clip_parse_config(yajl_val config);

#endif
