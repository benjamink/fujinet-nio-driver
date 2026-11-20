#ifndef FUJINET_NIO_SERIAL_WAIT_H
#define FUJINET_NIO_SERIAL_WAIT_H

#include <exec/io.h>

/* Both requests have been submitted. Returns 1 for timeout, 0 for read
 * completion (including read errors); both replies are consumed on return. */
int fujinet_serial_wait_read(struct IORequest *read_request,
                             struct IORequest *timer_request, ULONG signals);

#endif
