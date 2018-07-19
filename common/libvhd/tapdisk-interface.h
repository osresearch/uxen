
#include "tapdisk.h"

struct tiocb;

typedef void (*td_queue_callback_t)(void *arg, struct tiocb *, int err);

struct tiocb {
	td_queue_callback_t   cb;
	void                 *arg;

  // 	struct iocb           iocb;
	void                 *opaque;
	struct tiocb         *next;
};

void td_forward_request(td_request_t);
void td_complete_request(td_request_t, int);
void td_queue_tiocb(td_driver_t *, struct tiocb *);
void td_prep_read(struct tiocb *, int, char *, size_t,
		  long long, td_queue_callback_t, void *);
void td_prep_write(struct tiocb *, int, char *, size_t,
		   long long, td_queue_callback_t, void *);
