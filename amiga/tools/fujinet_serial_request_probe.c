#include <devices/serial.h>
#include <dos/dos.h>
#include <exec/errors.h>
#include <exec/nodes.h>
#include <clib/alib_protos.h>
#include <proto/exec.h>
#include <stdio.h>
#include <string.h>

#include "fujinet_serial_device.h"

/* Exercise the real resident device and Exec, without sending wire traffic.
 * The broker must not have opened the serial device yet. */
int fujinet_serial_request_probe(void)
{
    struct MsgPort *port = CreatePort(NULL, 0);
    struct IOExtSer req;
    UBYTE data = 0;
    unsigned i, failures = 0;

    if (port == NULL) return RETURN_FAIL;
    memset(&req, 0, sizeof(req));
    req.IOSer.io_Message.mn_ReplyPort = port;
    req.IOSer.io_Message.mn_Length = sizeof(req);
    if (OpenDevice((CONST_STRPTR)FUJINET_SERIAL_DEVICE_NAME, 0,
                   (struct IORequest *)&req, 0) != 0) {
        DeletePort(port);
        return RETURN_FAIL;
    }
    /* Seed an actual completed reply, not a fabricated ln_Type. */
    req.IOSer.io_Command = CMD_CLEAR;
    SendIO((struct IORequest *)&req);
    WaitIO((struct IORequest *)&req);
    for (i = 0; i < 64; ++i) {
        UBYTE type;
        int pending;
        LONG error;

        req.IOSer.io_Command = (i & 1) ? CMD_WRITE : CMD_READ;
        req.IOSer.io_Data = &data;
        req.IOSer.io_Length = 1;
        /* Hold interrupts only over submission/check/abort. This makes the
         * pending window deterministic and prevents the write hitting Paula.
         * No WaitIO or DOS calls are allowed inside this window. */
        Disable();
        SendIO((struct IORequest *)&req);
        type = req.IOSer.io_Message.mn_Node.ln_Type;
        pending = CheckIO((struct IORequest *)&req) == NULL;
        /* Abort unconditionally: the old driver falsely reports completion. */
        AbortIO((struct IORequest *)&req);
        Enable();
        error = WaitIO((struct IORequest *)&req);
        if (!pending || type != NT_MESSAGE || error != IOERR_ABORTED ||
            req.IOSer.io_Actual != 0 || GetMsg(port) != NULL) {
            ++failures;
            printf("REUSE index=%u command=%u pending=%d type=%u error=%ld\n",
                   i, (unsigned)req.IOSer.io_Command, pending, type, error);
        }
    }
    CloseDevice((struct IORequest *)&req);
    DeletePort(port);
    printf("%s serial-request-reuse trials=64 failures=%u\n",
           failures ? "FAIL" : "PASS", failures);
    return failures ? RETURN_FAIL : RETURN_OK;
}
