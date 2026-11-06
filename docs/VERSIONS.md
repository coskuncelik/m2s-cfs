# Source versions

Every source is a git submodule pinned to a full commit ID. `scripts/00-basic-setup.sh` checks all of
the values below and fails on any mismatch. Changing any of them needs explicit user approval
(see `docs/DECISIONS.md`).

Resolved 2026-11-03.

## Direct submodules of m2s-cfs

| Path | Repository | Commit | Label |
|---|---|---|---|
| `sources/cFS` | https://github.com/nasa/cFS | `c62b5b9c507af4972aad09ee9f3443fa16a9fc85` | 2025-07-07, `equuleus-rc1-108-gc62b5b9` |
| `sources/FreeRTOS-Kernel` | https://github.com/FreeRTOS/FreeRTOS-Kernel | `8a59a4b19bf72977819feddef87d41163ced7004` | tag `V11.3.2` (tag object `47797b7f41188341a360e3986346582135c0f866`), 2026-10-16 |
| `sources/references/mission-v0` | https://github.com/projetoChi2p/fs-nasa-cfs-mission-v0 | `ae4fe23e68735b8717ac0afeb0355fd863cd6ded` | reference only, 2026-05-04 |
| `sources/references/mission-v1` | https://github.com/projetoChi2p/fs-nasa-cfs-mission-v1 | `01acb7ee267162786d86c1e75d3f60a83b760e16` | reference only, 2026-04-20 |

## Nested submodules (pinned by cFS `c62b5b9`)

| Path in `sources/cFS` | Commit | Version in source |
|---|---|---|
| `cfe` | `fc8c48ad09460ce0277ad2bfcd81d376634aa2c6` | `equuleus-rc1+dev247` (`cfe_version.h`: `CFE_BUILD_NUMBER 247`); CHANGELOG top entry is dev251 |
| `osal` | `a77b5bd75d611670a6292f0a68922274d3dbf33f` | `equuleus-rc1+dev117` (`osapi-version.h`: `OS_BUILD_NUMBER 117`) |
| `psp` | `ae0c0a77e9a6e67eeab4110e346bb29b1cf64fad` | `equuleus-rc1+dev73` (`psp_version.h`: `CFE_PSP_IMPL_BUILD_NUMBER 73`) — see D1 |
| `apps/ci_lab` | `8c43b9eb45f2fe9ee5a8069cf07b2b45e3669971` | |
| `apps/sample_app` | `46279d0eeadf7cfa605888acc4db6c64193e4e88` | NASA sample app (not our `counter_app`) |
| `apps/sch_lab` | `dc58baf4f02e8adde0b411c6544c0f43a13de8d0` | |
| `apps/to_lab` | `fe6c9fd513f32c45615494554bafdbb646fcba6b` | |
| `libs/sample_lib` | `83d26957beaf256bbcdff3188a67dc4025726704` | |
| `tools/cFS-GroundSystem` | `c019d1bed5ffd9f6b3249bec37cc56b92bb36e0b` | |
| `tools/elf2cfetbl` | `fccae58fb587822a5e5c7a4de968de3d1dbb3ee9` | |
| `tools/tblCRCTool` | `42de9c416750e5c92575c74755fc4462a49a5b35` | |

## Submodules deliberately not initialized

- FreeRTOS-Kernel `portable/ThirdParty/Community-Supported-Ports` (`bae4c7aa…`) and
  `Partner-Supported-Ports` (`fccbbce9…`): the POSIX and ARM_CM3 ports we need are in the kernel repo itself.
- The reference missions' nested submodules: their URLs use SSH (`git@github.com:`), and they are
  reference material only.

## Host toolchain for the Cortex-M3 build and emulator (steps 08 and 09)

Installed by the user from Ubuntu 24.04 (noble/universe) on 2026-11-05; not pinned by script 00. Script 01
reports it, script 08 logs the version it used.

| Package | Version |
|---|---|
| `gcc-arm-none-eabi` | `15:13.2.rel1-2` (arm-none-eabi-gcc 13.2.1 20231009) |
| `binutils-arm-none-eabi` | `2.42-1ubuntu1+23` |
| `libnewlib-arm-none-eabi` | `4.4.0.20231231-2` (newlib-nano: `nano.specs`) |
| `qemu-system-arm` (step 09) | `1:8.2.2+ds-0ubuntu1.18` (QEMU 8.2.2, machine `mps2-an385`) |
| `gdb-multiarch` (step 09, debugging) | `15.1-1ubuntu1~24.04.1` |

## Notes

- FreeRTOS `V11.3.3` (2026-10-30) exists. We are not using it (no upgrade without approval).

## Other pinned material

| Item | Source | Revision |
|---|---|---|
| `.claude/skills/karpathy-guidelines/SKILL.md` | https://github.com/multica-ai/andrej-karpathy-skills `skills/karpathy-guidelines/SKILL.md` (MIT) | `2c606141936f1eeef17fa3043a72095b4765b9c2`, sha256 `6e22cc54cb02a5e98ae42d06d9d7292db0c1b43894831b32879beb0166b2aea7` |
