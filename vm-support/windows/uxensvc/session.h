#ifndef SESSION_H_
#define SESSION_H_

void session_connect(DWORD session_id);
void session_disconnect(DWORD session_id);

extern CRITICAL_SECTION session_lock;

#endif /* SESSION_H_ */
