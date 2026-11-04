# CLAUDE.md — m2s-cfs

Read this file and `.claude/skills/karpathy-guidelines/SKILL.md` before doing anything else.
The Karpathy guidelines apply to all work. Where they conflict with the safety, static-memory,
aerospace or verification requirements below, the requirements below win.

## Project

`m2s-cfs` ports NASA cFS/cFE to FreeRTOS on two targets:

- **Target A — Linux simulator:** real FreeRTOS Kernel V11.3.2 POSIX port (not the native POSIX OSAL).
- **Target B — SmartFusion2 M2S150-ADV-DEV-KIT:** ARM Cortex-M3, FreeRTOS V11.3.2 `GCC/ARM_CM3`,
  `arm-none-eabi-gcc`, static memory only, output on the physical UART, JTAG debugging.

The same `counter_app` source runs on both targets and prints through `CFE_ES_WriteToSysLog`.

Full requirements: `prompt.md`. Versions: `docs/VERSIONS.md`. Decisions: `docs/DECISIONS.md`.
Current progress and next actions: `docs/STATUS.md` (read it when resuming; keep it updated after each step).

## Pinned sources

All sources are git submodules under `sources/`, pinned to the full commit IDs in `docs/VERSIONS.md`.
`scripts/00-basic-setup.sh` sets them up and verifies them.

- Never upgrade, replace or re-pin a dependency without explicit user approval.
- Never edit files inside `sources/`. Project changes go in documented patches (`patches/`) or
  project-owned code and configuration.

## Mandatory workflow (one step at a time)

For each step: explain the objective → state assumptions → inspect the sources → present the plan and the
files to touch → list risks → define measurable success criteria → **wait for explicit approval** →
implement only that step → run the tests → report the real output and exit status → update the docs →
**wait for approval before the next step**.

Never implement future phases early. The latest explicit user decision always wins.

## Safety rules

Without explicit approval, do not:

- install packages or run `sudo`;
- clone or download anything;
- change branches, commits or submodule revisions;
- reset, clean or discard git changes;
- commit or push;
- program, erase or otherwise touch physical hardware.

## Scripts

Write scripts in Bash with `set -euo pipefail`. Each script must check its prerequisites, fail with a
clear message and the right exit code, log to `logs/`, and be idempotent where possible.

## Engineering rules

- Embedded runtime: no dynamic allocation. Back this with evidence (link map, `malloc`/`_sbrk` trap).
  Do not rely only on `configSUPPORT_STATIC_ALLOCATION`.
- Keep Linux host-side allocations clearly separate from embedded runtime allocations.
- No platform-specific UART code in applications. Output goes cFE → OSAL console → BSP.
- Do not fabricate test results, versions or hardware capabilities. In reports, separate verified facts,
  reasoned conclusions, assumptions and open questions.
