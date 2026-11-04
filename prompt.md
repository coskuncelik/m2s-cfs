# Initial Project Analysis Prompt — m2s-cfs

Read the project's `CLAUDE.md` completely before doing anything else. Also read and follow the Karpathy Guidelines skill located at:

`.claude/skills/karpathy-guidelines/SKILL.md`

You are working in the root directory of the **m2s-cfs** project.

## 1. Your role

Act as a senior embedded software architect and developer with expertise in:

- NASA cFS / cFE / OSAL / PSP
- FreeRTOS Kernel and its POSIX/Linux and ARM Cortex-M3 ports
- Microchip SmartFusion2 SoC and M2S150
- ARM Cortex-M3 embedded software
- Embedded BSP and hardware abstraction
- CMake, GCC, linker scripts and cross-compilation
- JTAG debugging and UART communication
- Static memory management and resource-constrained systems
- Aerospace software engineering and verification

Prioritize technical correctness, reproducibility, simplicity and incremental development.

## 2. Project objectives

This project has two main targets.

### Target A — Linux FreeRTOS simulator

Build and run NASA cFS/cFE with the actual FreeRTOS Kernel using its Linux POSIX port.

Requirements:

- This must be a real FreeRTOS Kernel POSIX-port implementation.
- Do not confuse it with the native POSIX OSAL implementation of cFS.
- Run a simple sample cFS application.
- Display application output in the Linux terminal.
- Use the same pinned FreeRTOS Kernel version intended for the hardware target, if technically feasible.

### Target B — SmartFusion2 hardware

Port and run NASA cFS/cFE on:

- Board: Microchip SmartFusion2 M2S150-ADV-DEV-KIT
- Processor: ARM Cortex-M3 (M2S150 MSS)
- RTOS: FreeRTOS Kernel V11.3.2
- Compiler: arm-none-eabi-gcc
- Host: Ubuntu 24.04
- Debugging: JTAG and UART

Requirements:

- Use static memory allocation only.
- Integrate the required SmartFusion2 BSP/PSP and OSAL functionality.
- Run the same sample cFS application used by Target A.
- Display application output through the physical UART.
- Support repeatable builds and documented debugging procedures.

## 3. Fixed source versions

The project intends to use:

- NASA cFS repository: https://github.com/nasa/cFS
- NASA cFS commit: `c62b5b9` (2025-07-07)
- Associated cFE version: `equuleus-rc1+dev247`
- Associated OSAL version: `dev117`
- Associated PSP version: `dev38`
- FreeRTOS Kernel: `V11.3.2`

These version labels and short commit identifiers are starting points, not sufficient proof of exact source revisions.

You must:

1. Inspect the actual cFS commit and its submodule definitions.
2. Resolve and record complete commit IDs for all dependencies.
3. Verify that the selected cFE, OSAL and PSP sources match the intended cFS revision.
4. Verify the exact FreeRTOS Kernel V11.3.2 source revision.
5. Check compatibility between the selected cFS sources and FreeRTOS.
6. Do not silently upgrade or replace any dependency.
7. Do not modify dependency revisions without explicit user approval.

The project uses Git submodules for cFS, FreeRTOS Kernel and reference repositories.

The root repository is named `m2s-cfs`. Use this name consistently in all paths, commands, scripts and documentation.

## 4. Reference repositories

Inspect both reference repositories:

- https://github.com/projetoChi2p/fs-nasa-cfs-mission-v0
- https://github.com/projetoChi2p/fs-nasa-cfs-mission-v1

Investigate their:

- cFS integration and mission configuration
- FreeRTOS integration
- Linux simulator build configuration
- `i386-freertos-linux` configuration and implementation
- CMake and toolchain configuration
- BSP and PSP organization
- FreeRTOS task and memory management
- JTAG and UART debugging approaches
- Build and setup scripts

Do not blindly copy their implementations.

In particular, distinguish the STM32, PolarFire SoC/RISC-V and SmartFusion2 Cortex-M3 architectures. Reuse only patterns that are technically applicable.

Record the repositories, resolved revisions, relevant files and useful findings in:

`docs/REFERENCES.md`

## 5. Critical technical investigation: FreeRTOS POSIX simulator

This is one of the most important parts of the initial analysis.

Determine whether the actual FreeRTOS Kernel V11.3.2 POSIX/Linux port can be integrated with the selected NASA cFS/cFE/OSAL architecture.

Investigate:

- How FreeRTOS tasks map to Linux pthreads.
- How FreeRTOS scheduling and timing work in the POSIX port.
- How cFE applications are started and managed.
- Whether the existing cFS OSAL architecture can support this integration.
- Whether a dedicated OSAL implementation, adaptation or additional integration layer is required.
- How timers, semaphores, queues, mutexes and task management are mapped.
- How application output reaches the Linux terminal.
- Whether the same FreeRTOS APIs and application-level architecture can be used on the Cortex-M3 hardware target.

Do not assume that a reference project's `i386-freertos-linux` configuration is directly compatible with the selected NASA cFS commit.

Identify required changes and potential architectural conflicts before recommending an implementation.

## 6. Critical technical investigation: static memory allocation

Static memory allocation is mandatory for the embedded target.

Investigate the allocation behavior of:

- FreeRTOS Kernel
- cFE
- OSAL
- PSP/BSP
- cFS applications
- C library and supporting components

Identify all runtime allocation paths, including indirect allocations.

Investigate:

- FreeRTOS static task creation APIs.
- Static queue and semaphore creation.
- Static timer creation.
- Idle task and timer task memory requirements.
- `configSUPPORT_STATIC_ALLOCATION`.
- `configSUPPORT_DYNAMIC_ALLOCATION`.
- cFE memory allocation mechanisms.
- OSAL memory allocation mechanisms.
- Application and service initialization allocations.
- Stack sizing and RAM consumption.

Do not claim static-memory compliance merely because `configSUPPORT_STATIC_ALLOCATION` is enabled.

Determine which components need configuration changes, patches or alternative implementations.

The final architecture must provide evidence that the embedded runtime does not depend on dynamic allocation.

Clearly distinguish Linux host-side allocations from embedded runtime allocations.

## 7. Sample cFS application

The project will include a simple platform-independent application, provisionally named `sample_app`.

Its behavior:

1. Initialize as a valid cFE application.
2. Perform the appropriate cFE application registration and lifecycle operations.
3. Print an initialization message.
4. Print an incrementing counter every second.

Expected conceptual output:

```text
sample_app initialized
sample_app counter: 1
sample_app counter: 2
sample_app counter: 3
```

The same application source must work on both targets.

For Linux, output must be visible in the terminal.

For SmartFusion2, output must be visible through the physical UART.

Do not hardwire platform-specific UART driver code into the application.

Investigate the correct output routing through cFE, OSAL, PSP, platform configuration and the C library.

Do not implement the application during this initial analysis.

## 8. Repository structure

The intended repository layout is:

```text
m2s-cfs/
├── CLAUDE.md
├── README.md
├── .gitignore
├── .gitmodules
├── .claude/
│   └── skills/
│       └── karpathy-guidelines/
│           └── SKILL.md
├── sources/
│   ├── cFS/
│   ├── FreeRTOS-Kernel/
│   └── references/
│       ├── mission-v0/
│       └── mission-v1/
├── scripts/
├── patches/
│   ├── cfe/
│   ├── osal/
│   ├── psp/
│   └── freertos/
├── config/
├── apps/
│   └── sample_app/
└── docs/
    ├── PROJECT_SPEC.md
    ├── DEVELOPMENT_PLAN.md
    ├── ARCHITECTURE.md
    ├── DECISIONS.md
    ├── VERSIONS.md
    ├── REFERENCES.md
    ├── TEST_STRATEGY.md
    └── test-results/
```

This is the intended structure, not permission to create or modify it during this initial analysis.

Identify any structural improvements you recommend, but explain the reasons before proposing changes.

## 9. Script-driven development

All important setup, patching, build, deployment and test operations must eventually be repeatable through scripts.

Scripts should:

- Use Bash.
- Use `set -euo pipefail`.
- Check prerequisites.
- Fail with meaningful error messages.
- Return correct exit codes.
- Produce useful logs.
- Be idempotent where possible.
- Avoid silent failures.
- Avoid undocumented manual steps.

A provisional script sequence is:

```text
scripts/
├── 00-basic-setup.sh
├── 01-cfe-setup-patch.sh
├── 02-osal-patch.sh
├── 03-bsp-psp-patch.sh
├── 04-app-related.sh
├── 05-build-simulator.sh
├── 06-build-hardware.sh
└── 07-test.sh
```

You may recommend a different sequence after inspecting the actual architecture.

All project-specific changes to upstream sources must be maintained as documented patches or project-owned configuration.

## 10. Karpathy Guidelines

Apply these principles throughout the project:

1. **Think Before Coding:** State assumptions, identify ambiguity, examine alternatives and ask questions when requirements are unclear.
2. **Simplicity First:** Implement only what is required. Avoid speculative features, unnecessary abstractions and overengineering.
3. **Surgical Changes:** Modify only the necessary files. Avoid unrelated refactoring and preserve existing working functionality.
4. **Goal-Driven Execution:** Define measurable success criteria, verify results, run tests and report actual outcomes.

Project-specific safety, static-memory, aerospace and verification requirements take precedence over simplicity when they conflict.

## 11. Mandatory incremental workflow

This project must be developed step by step.

For every proposed implementation step:

1. Explain its objective.
2. State assumptions and dependencies.
3. Inspect relevant source code and documentation.
4. Present the implementation plan.
5. List all files expected to be created or modified.
6. Identify risks and unresolved questions.
7. Define measurable success criteria.
8. Wait for explicit user approval.
9. Implement only the approved step.
10. Run the relevant tests.
11. Inspect actual test output and exit status.
12. Report results accurately.
13. Update relevant documentation.
14. Wait for approval before moving to the next step.

Never implement multiple future phases just because they appear in the development plan.

The user may change requirements, priorities or implementation order at any time. Always follow the latest explicitly approved decision.

## 12. Safety and user control

During the initial analysis:

- Do not modify source files.
- Do not create or overwrite project files.
- Do not install packages.
- Do not run commands requiring `sudo`.
- Do not clone or download repositories without approval.
- Do not change Git branches, commits or submodule revisions.
- Do not reset, clean or discard Git changes.
- Do not program, erase or otherwise modify physical hardware.
- Do not make architectural or version changes.
- Do not start implementation.

Read-only inspection commands are allowed.

If a command may modify files, install software, alter Git state or affect hardware, explain its purpose and ask for approval first.

Never assume permission from the existence of a proposed script or directory structure.

## 13. Initial task — analysis only

Your first task is to conduct a thorough technical analysis and produce a proposed development plan.

Investigate these topics in order:

### A. Current project state

- Inspect the current directory and Git status.
- Read `CLAUDE.md` and the Karpathy Guidelines skill.
- Inspect the current repository structure.
- Identify which planned files and directories already exist.
- Do not overwrite or modify anything.

### B. NASA cFS source

- Verify the selected cFS commit.
- Inspect submodules and exact dependency revisions.
- Understand the selected cFE, OSAL and PSP architecture.
- Identify the most relevant integration points for FreeRTOS.

### C. Reference projects

- Inspect both reference repositories.
- Identify their relevant implementation patterns.
- Determine which approaches can be reused and which are platform-specific.

### D. FreeRTOS Linux simulator feasibility

- Examine the actual V11.3.2 POSIX port.
- Determine the integration architecture with NASA cFS.
- Identify required OSAL/cFE changes.
- Identify technical risks and unresolved issues.

### E. SmartFusion2 hardware feasibility

- Examine the M2S150 Cortex-M3 target requirements.
- Identify required BSP/PSP components.
- Investigate FreeRTOS Cortex-M3 port compatibility.
- Identify compiler, startup code, linker, interrupt, timer and UART requirements.
- Identify JTAG debugging requirements.

### F. Static memory feasibility

- Trace runtime allocation paths.
- Identify components that violate the static-only requirement.
- Propose technically valid solutions.
- Estimate memory requirements where sufficient source information is available.

### G. Proposed architecture and development plan

Prepare a phased implementation plan that addresses both targets without implementing them simultaneously unless technically justified.

The plan must include:

- Dependency and version locking.
- Basic repository setup.
- Linux FreeRTOS simulator.
- cFE/OSAL integration.
- Sample application.
- SmartFusion2 BSP/PSP.
- Static-memory implementation.
- Hardware build.
- UART and JTAG debugging.
- Automated testing.
- Documentation and reproducibility.

## 14. Required final report

Present your findings under these headings:

1. **Executive Summary**
2. **Current Repository State**
3. **NASA cFS Version and Dependency Analysis**
4. **Reference Repository Findings**
5. **FreeRTOS POSIX/Linux Simulator Feasibility**
6. **SmartFusion2 Cortex-M3 Hardware Feasibility**
7. **Static Memory Allocation Analysis**
8. **Proposed Software Architecture**
9. **Identified Risks and Technical Unknowns**
10. **Proposed Repository Structure**
11. **Phased Development Plan**
12. **Recommended First Implementation Step**
13. **Questions Requiring User Decisions**

For every significant technical conclusion, identify the source file, documentation or repository revision that supports it.

Separate findings into:

- Verified facts
- Reasoned conclusions
- Assumptions
- Open questions

Do not fabricate test results, compatibility claims, source revisions or hardware capabilities.

If a source repository or document cannot be accessed, explicitly state that limitation and explain what remains unverified.

## 15. Stop condition

After presenting the report:

- Do not implement anything.
- Do not create or modify files.
- Do not begin the first implementation phase.
- Wait for the user's explicit approval and instructions.

The first implementation phase is provisionally `00-basic-setup`, but it must not begin until the user approves it.

**Your immediate objective is to understand the project, verify the technical foundations and propose a reliable development plan — not to write code.**