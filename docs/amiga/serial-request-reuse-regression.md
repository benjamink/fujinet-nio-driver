# Serial request reuse regression (2026-11-20)

FujiBlast's physical log recorded one failed 15-byte network write after about
49 seconds: `rc=16 broker=2/16 cause=7 serial=250 status=0`, with no malformed
frames or queue saturation. Cause 7 is the serial read of the reply; 250 is
signed `IOERR_UNITBUSY` (-6). This is not the game's silence threshold. The
user confirmed `fujinet-serial.device 0.11` was selected.

## Confirmed defect and fix

The serial driver's BeginIO did not mark a reused IORequest as `NT_MESSAGE`.
Its previous `ReplyMsg` left `NT_REPLYMSG` behind. Exec's CheckIO/WaitIO could
therefore report completion while the driver's read or write was still pending.
Reusing that request can produce UNITBUSY and corrupt request ownership.
An interrupt completing quickly can conceal the defect.

BeginIO now sets `NT_MESSAGE` and clears io_Error before dispatch. This follows
the [Amiga device contract](https://wiki.amigaos.net/wiki/Example_Device).
Serial version is 0.12, with numeric revision matching the identification string.

The broker also previously interpreted a single Wait wake-up as completion or
deadline. Port signals can remain set from previous replies. It now checks both
requests, waits again on a stale signal, prefers an already completed read, and
consumes both replies before returning. Broker version is 0.10. Neither wire
protocol nor public client ABI changes; FujiBlast still uses released lib 0.2.0.

These are reproduced software defects consistent with the physical log, not
proof that every physical disconnect has the same cause. A physical retest is
still required. No baud or game timeout increase is part of this correction.

## Regression and validation

`fujinet-nio-exchange --serial-request-test` must run in an isolated session,
before the broker opens serial. It opens the real custom driver, seeds a genuine
completed request, then submits and aborts 64 alternating reads/writes. Interrupts
are briefly held over submit/check/abort so no outgoing test bytes reach Paula;
all waits and logging run after interrupts are restored. Each pending request
must remain incomplete to Exec, then reply once on abort with zero transferred
bytes. The old driver fails all 64 checks; the fixed driver passes all 64.

Native wait tests cover two stale wake-ups before read/deadline completion,
simultaneous completions, already completed requests, error preservation, and
exactly one consumed reply for each request. The existing lifecycle and UART
tests remain passing.

From the workspace, after `source scripts/env.sh`:

```sh
make -C repos/fujinet-nio-driver amiga-tests
uv run --project integration-tests/amiberry pytest --run-amiga \
  --amiga-env wb32 --amiga-machine a1200-030 \
  integration-tests/amiberry/test_nio_paula_serial.py -q
```

The expanded guest case also checks an ordinary clock exchange and 100 warm
clock exchanges over the POSIX NIO serial path. Red evidence:
`test-evidence/amiberry-20261120-120534`; passing expanded WB3.2 evidence:
`test-evidence/amiberry-20261120-120704` (19.80 seconds).

The first expanded real-speed WB1.3 run reached request 101 but exceeded the
old 60-second harness activity budget before the completion marker. Only that
variant now allows 120 seconds of activity / 150 seconds overall for 100 trials;
no serial, broker, or game timeout was increased.

The final WB1.3/68000 run passed in 99.68 seconds, evidence
`test-evidence/amiberry-20261120-120914`, using:

```sh
uv run --project integration-tests/amiberry pytest --run-amiga \
  --amiga-env wb13 --amiga-machine a500-000 \
  integration-tests/amiberry/test_nio_paula_serial.py -q
```

Both profiles pass all 64 pending-request checks and 100 measured warm trials
(each warm trial also sends a warm-up clock request).

## Physical retest

Use the matching Workbench profile under `build/amiga/wb32/` (or `wb13/`).
Install both `fujinet-serial.device` and `fujinet-nio.device` in the directory
used by the startup resident-loader commands, then reboot. Copying a file alone
does not replace the already resident device. Check:

```text
Version fujinet-serial.device FULL
Version fujinet-nio.device FULL
```

Expected versions are 0.12 and 0.10 respectively. Keep the current baud initially
and play using the existing FujiBlast binary. Preserve `RAM:fujiblast-net.log`
before any further reboot if a disconnect occurs.
