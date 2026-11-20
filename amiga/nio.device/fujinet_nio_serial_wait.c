#include <proto/exec.h>
#include "fujinet_nio_serial_wait.h"

int fujinet_serial_wait_read(struct IORequest *read_request,
                             struct IORequest *timer_request, ULONG signals)
{
    /* Signals are hints, not completion records. A previous reply can leave
     * its port signal set, and both requests can finish before we wait. */
    while (CheckIO(read_request) == NULL && CheckIO(timer_request) == NULL)
        Wait(signals);

    /* Prefer a completed read if the timer also finished. */
    if (CheckIO(read_request) == NULL) {
        AbortIO(read_request);
        WaitIO(read_request);
        WaitIO(timer_request);
        return 1;
    }
    WaitIO(read_request);
    if (CheckIO(timer_request) == NULL)
        AbortIO(timer_request);
    WaitIO(timer_request);
    return 0;
}
