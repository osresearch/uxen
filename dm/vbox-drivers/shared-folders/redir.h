#ifndef _SF_REDIR_H_
#define _SF_REDIR_H_

int sf_redirect_add(wchar_t *sfname, wchar_t *src, wchar_t *dst);
int sf_redirect_del(wchar_t *sfname, wchar_t *src);
void sf_redirect_init(void);

#endif
