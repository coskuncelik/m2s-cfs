# Test result — 04b static RAM file system on the Linux simulator

- Date: 2026-11-04
- Host: Ubuntu 24.04.5 LTS x86_64 in a Docker container (2 vCPUs, Intel Xeon Platinum 8370C), kernel
  6.8.0-1064-azure, glibc 2.39, gcc 13.3.0, cmake 3.28.3
- Sources (unmodified in `sources/`): OSAL `a77b5bd75d611670a6292f0a68922274d3dbf33f`,
  FreeRTOS-Kernel `8a59a4b19bf72977819feddef87d41163ced7004` (V11.3.2)
- Project code: `osal-ext/freertos/inc/os-ramfs.h`, `osal-ext/freertos/src/os-ramfs.c` (core),
  `osal-ext/freertos/src/os-impl-filesys.c` (OSAL glue; replaces `os-impl-no-filesys.c`),
  settings in `osal-ext/freertos/inc/os-freertos.h`, core test `tests/ramfs-core/`. Design: D19, D25,
  `docs/ARCHITECTURE.md`.
- Command: `ROUNDS=3 ./scripts/04-build-osal-freertos-sim.sh --clean` — exit 0, 5 min 55 s

## Results

| Check | Result |
|---|---|
| Clean build of OSAL + 14 tests (project code `-Wall -Wextra -Werror`) | PASS |
| RAM file system core test (host gcc, `-Werror`, AddressSanitizer + UndefinedBehaviorSanitizer) | PASS — 109/109 checks |
| No FreeRTOS heap or dynamic object creation in any test binary | PASS |
| 14 tests × 3 rounds, no FAIL/TSF/TTF except D22 | PASS — 42/42 |
| `sources/cFS` and `sources/FreeRTOS-Kernel` unchanged and clean | PASS |

New OSAL tests (identical in all 3 rounds; no N/A or skipped assertions):

| Test | Assertions | Result |
|---|---|---|
| file-api-test (mkfs/mount, create/remove, open/close, chmod, read/write/lseek, mkdir/rmdir and free blocks, directory listing, stat, open-file API, unmount/remount, rename) | 140 | PASS |
| file-sys-add-fixed-map-api-test | 8 | PASS |

The 12 tests from 04a gave the same results as before (`04a-osal-freertos-sim.md`), including the one tolerated
osal-core-test assertion (D22). Volume stats in file-api-test looked right: 193 of the 200 requested 512-byte
blocks were free on the new disk (the rest hold the entry and block tables), 191 with two small files open.

## Core test coverage (`tests/ramfs-core/ramfs_core_test.c`)

Format and attach (misaligned memory, bad arguments, unformatted memory); paths (extra slashes, `.`, `..`
rejected, 19-character names accepted and 20 rejected); create/lookup/remove errors (exists, missing parent,
file as parent, non-empty directory, root); directory listing; rename (move a file, rename a non-empty directory,
replace a file, refuse a directory into itself, refuse file/directory replacement); 2000 random writes, seeks and
reads checked byte for byte against a reference copy; zeros after a write past the end, also in reused blocks;
volume full (partial write, then `RAMFS_ERR_NO_SPACE`, no leaked blocks); entry table full; contents kept on
re-attach; damaged size and damaged magic detected.

The test was checked against four deliberately broken copies of the core (no clean-up after a failed write, no
zeroing of reused blocks, no "directory not empty" check, no loop check in rename); it failed on every one. The
first version of the test missed the zeroing bug; a check was added.

## Dynamic memory evidence (host build)

`nm -u` over every object: only the POSIX port (`port.c`, `wait_for_event.c`) and the OSAL test harness
(`ut_assert` `utlist.c`, `utstubs.c`) reference `malloc`/`free`, as in 04a. The two new objects reference only
string/memory functions, FreeRTOS, OSAL and the host stack protector (`__stack_chk_fail`, Ubuntu's default).

New static memory (simulator settings): RAM pool 256 KiB, root volume 32 KiB, open-file table 1000 B, open-directory
table and volume table under 1 KiB.

## Notes and open points

- Facts: the OSAL file tests pass with no tolerated failures; nothing in the file system allocates memory.
- Not covered by our core test: the open-file limit. That limit is OSAL's own stream table (`OS_MAX_NUM_OPEN_FILES`);
  our handle table is indexed by the same token, so it cannot overflow (checked by reading the code).
- Not tested yet: OSAL's API-exhaustive file unit tests (`src/unit-tests/osfile-test`, `osfilesys-test`), which are 04c.
- Not tested: concurrent file access from several tasks (one mutex serialises all calls, by design).
- Open question for the board: RAM sizes on the M2S150 kit. All sizes are compile-time settings.
- Deferred to the cFE step (D5): built-in read-only files for the cFE startup script.
