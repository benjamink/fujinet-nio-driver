#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <exec/nodes.h>
#include "fujinet_nio_serial_wait.h"

static struct IORequest requests[2];
static unsigned waits, aborts[2], replies[2], scenario;

struct IORequest *CheckIO(struct IORequest *req)
{
    return req->io_Message.mn_Node.ln_Type == NT_REPLYMSG ? req : NULL;
}

LONG AbortIO(struct IORequest *req)
{
    unsigned index = (unsigned)(req - requests);
    ++aborts[index];
    req->io_Message.mn_Node.ln_Type = NT_REPLYMSG;
    return 0;
}

LONG WaitIO(struct IORequest *req)
{
    assert(CheckIO(req) != NULL);
    ++replies[req - requests];
    return req->io_Error;
}

ULONG Wait(ULONG signals)
{
    assert(signals == 3);
    assert(++waits <= 3);
    /* Two stale/spurious wakes before a genuine completion. */
    if (waits == 3) {
        if (scenario == 0 || scenario == 2)
            requests[0].io_Message.mn_Node.ln_Type = NT_REPLYMSG;
        if (scenario == 1 || scenario == 2)
            requests[1].io_Message.mn_Node.ln_Type = NT_REPLYMSG;
    }
    return signals;
}

static void run(unsigned which, unsigned already_done)
{
    unsigned i;
    scenario = which;
    waits = 0;
    memset(requests, 0, sizeof(requests));
    memset(aborts, 0, sizeof(aborts));
    memset(replies, 0, sizeof(replies));
    for (i = 0; i < 2; ++i)
        requests[i].io_Message.mn_Node.ln_Type =
            (already_done & (1U << i)) ? NT_REPLYMSG : NT_MESSAGE;
    /* Completion with a read error must still reclaim both requests. */
    requests[0].io_Error = -6;
    assert(fujinet_serial_wait_read(&requests[0], &requests[1], 3) ==
           (which == 1));
    assert(waits == (already_done ? 0U : 3U));
    assert(replies[0] == 1 && replies[1] == 1);
    assert(aborts[0] == (which == 1));
    assert(aborts[1] == (which == 0));
    assert(requests[0].io_Error == -6);
}

int main(void)
{
    run(0, 0); /* stale signals then read */
    run(1, 0); /* stale signals then deadline */
    run(2, 0); /* simultaneous completion, read wins */
    run(0, 1); /* read completed before entering wait */
    run(1, 2); /* timer completed before entering wait */
    run(2, 3); /* both completed before entering wait */
    puts("PASS serial wait: stale signals, deadline, completion and cleanup");
    return 0;
}
