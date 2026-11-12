# scripts/lib/mem-probe-gdb.py — gdb (Python) helper for scripts/10-memory-report.sh. Not run on its own.
#
# Loaded by gdb-multiarch after it has connected to QEMU's gdb stub and halted the guest. Needs an image built
# with -g (types of TCB_t, cFE and OSAL globals). Reads, and never writes, guest memory. Prints "MEM ..." lines
# that script 10 turns into the report:
#   MEM STACK <owner> <base> <size> <used> <free>     task stacks (FreeRTOS fills new stacks with 0xA5)
#   MEM MSP <size> <used>                             main stack (start-up and interrupts; zero-filled RAM)
#   MEM POOL <name> <total> <peak> <allocs>           cFE ES memory pools (peak = TailPosition)
#   MEM SB <field> <value>                            cFE SB statistics (peaks kept by SB itself)
#   MEM CFE <name> <value>                            other cFE use figures
#   MEM OSAL <type> <in_use> <limit>                  OSAL objects in use at the end of the run
#   MEM QUEUE <index> <depth> <msg_size>              OSAL queues existing at the end of the run
#   MEM PSP <name> <value>                            PSP reserved memory figures
# Environment: M2S_SYMTAB = "addr size name" lines (hex, hex, name) of the image's RAM symbols (nm -S).
import os
import re

import gdb

INF = gdb.selected_inferior()
FILL = 0xA5          # tskSTACK_FILL_BYTE (configCHECK_FOR_STACK_OVERFLOW 2 sets new stacks to known value)
OSAL_TYPES = {1: "task", 2: "queue", 3: "countsem", 4: "binsem", 5: "mutex", 6: "stream", 7: "dir",
              8: "timebase", 9: "timecb", 10: "module", 11: "filesys", 12: "console", 13: "condvar"}


def ev(expr):
    return gdb.parse_and_eval(expr)


def rid(v):
    """Integer value of a cFE resource id (a struct or a plain integer, depending on the build)."""
    while v.type.strip_typedefs().code == gdb.TYPE_CODE_STRUCT:
        v = v[v.type.strip_typedefs().fields()[0]]
    return int(v)


def mem(addr, n):
    return bytes(INF.read_memory(addr, n))


def leading(data, value):
    n = 0
    for b in data:
        if b != value:
            break
        n += 1
    return n


def load_symtab():
    syms = []
    with open(os.environ["M2S_SYMTAB"]) as f:
        for line in f:
            a, s, name = line.split()[:3]
            syms.append((int(a, 16), int(s, 16), name))
    return syms


SYMS = load_symtab()


def sym_at(addr):
    for a, s, name in SYMS:
        if a <= addr < a + s:
            return a, s, name
    return None


def live_tasks():
    """pxStack -> task name, for every task on a FreeRTOS list (ready, delayed, pending, suspended, deleted)."""
    lists = ["pxReadyTasksLists[%d]" % i for i in range(int(ev("sizeof(pxReadyTasksLists)/sizeof(pxReadyTasksLists[0])")))]
    lists += ["xDelayedTaskList1", "xDelayedTaskList2", "xPendingReadyList", "xSuspendedTaskList",
              "xTasksWaitingTermination"]
    tcb_t = gdb.lookup_type("TCB_t").pointer()
    out = {}
    for name in lists:
        lst = ev(name)
        end = int(lst["xListEnd"].address)
        item = lst["xListEnd"]["pxNext"]
        for _ in range(int(lst["uxNumberOfItems"])):
            tcb = item["pvOwner"].cast(tcb_t)
            out[int(tcb["pxStack"])] = tcb["pcTaskName"].string()
            item = item["pxNext"]
            if int(item) == end:
                break
    return out


def stack_regions():
    """(base, size, label) of every task stack area the image has."""
    regions = []
    for a, s, name in SYMS:
        m = re.match(r"OS_FreeRTOS_Stack_(\d+)$", name)
        if m:
            per = int(m.group(1))
            for i in range(s // per):
                regions.append((a + i * per, per, "%s[%d]" % (name, i)))
        elif re.match(r"(uxIdleTaskStack|uxTimerTaskStack)(\.\d+)?$", name) or name in (
                "OS_FreeRTOS_ConsoleStack", "OS_BSP_MainStack"):
            regions.append((a, s, name))
    n = int(ev("sizeof(OS_impl_timebase_table)/sizeof(OS_impl_timebase_table[0])"))
    for i in range(n):
        st = ev("OS_impl_timebase_table[%d].stack" % i)
        regions.append((int(st.address), int(st.type.sizeof), "OS_impl_timebase_table[%d].stack" % i))
    return regions


def report_stacks():
    owners = live_tasks()
    for base, size, label in stack_regions():
        data = mem(base, size)
        if leading(data, 0) == size:
            print("MEM STACK %s:never-used 0x%08x %d 0 %d" % (label, base, size, size))
            continue
        free = leading(data, FILL)
        owner = owners.get(base, "(no live task)")
        print("MEM STACK %s:%s 0x%08x %d %d %d" % (label, owner.replace(" ", "_"), base, size, size - free, free))
    ebss = int(ev("(unsigned)&_ebss"))
    estack = int(ev("(unsigned)&_estack"))
    data = mem(ebss, estack - ebss)
    print("MEM MSP %d %d" % (estack - ebss, estack - ebss - leading(data, 0)))


def report_cfe():
    pools = ev("CFE_ES_Global.MemPoolTable")
    for i in range(pools.type.sizeof // pools[0].type.sizeof):
        p = pools[i]
        total = int(p["Pool"]["PoolTotalSize"])
        if total == 0:
            continue
        base = int(p["BaseAddr"])
        s = sym_at(base)
        print("MEM POOL %s %d %d %d" % (s[2] if s else "0x%08x" % base, total, int(p["Pool"]["TailPosition"]),
                                       int(p["Pool"]["AllocationCount"])))
    st = ev("CFE_SB_Global.StatTlmMsg.Payload")
    for f in st.type.fields():
        if f.name and ("InUse" in f.name or f.name.startswith("Max")):
            print("MEM SB %s %d" % (f.name, int(st[f.name])))
    pipes = ev("CFE_SB_Global.PipeTbl")
    for i in range(pipes.type.sizeof // pipes[0].type.sizeof):
        p = pipes[i]
        if rid(p["PipeId"]) != 0:          # pipes in use now (deleted ones keep their old figures)
            print("MEM SB pipe%d.MaxQueueDepth/PeakQueueDepth %d/%d" % (i, int(p["MaxQueueDepth"]),
                                                                       int(p["PeakQueueDepth"])))
    print("MEM CFE CDS.TotalSize %d" % int(ev("CFE_ES_Global.CDSVars.TotalSize")))
    print("MEM CFE sizeof(CFE_ES_ResetData_t) %d" % int(ev("sizeof(CFE_ES_ResetData_t)")))
    apps = ev("CFE_ES_Global.AppTable")
    used = sum(1 for i in range(apps.type.sizeof // apps[0].type.sizeof) if rid(apps[i]["AppId"]) != 0)
    print("MEM CFE apps-in-use %d" % used)
    print("MEM CFE TBL.NumTables-registered %d" % sum(
        1 for r in iter_array("CFE_TBL_Global.Registry") if rid(r["OwnerAppId"]) != 0))


def iter_array(expr):
    arr = ev(expr)
    for i in range(arr.type.sizeof // arr[0].type.sizeof):
        yield arr[i]


def report_osal():
    counts = {}
    active_queues = set()
    nq = int(ev("sizeof(OS_queue_table)/sizeof(OS_queue_table[0])"))
    for r in iter_array("OS_common_table"):
        oid = int(r["active_id"])
        if oid:
            t = OSAL_TYPES.get(oid >> 16, "type%d" % (oid >> 16))
            counts[t] = counts.get(t, 0) + 1
            if t == "queue":
                active_queues.add((oid & 0xFFFF) % nq)   # OS_ObjectIdToArrayIndex: serial modulo table size
    limits = {"task": "OS_MAX_TASKS", "queue": "OS_MAX_QUEUES", "countsem": "OS_MAX_COUNT_SEMAPHORES",
              "binsem": "OS_MAX_BIN_SEMAPHORES", "mutex": "OS_MAX_MUTEXES", "stream": "OS_MAX_NUM_OPEN_FILES",
              "dir": "OS_MAX_NUM_OPEN_DIRS", "timebase": "OS_MAX_TIMEBASES", "timecb": "OS_MAX_TIMERS",
              "module": "OS_MAX_MODULES", "filesys": "OS_MAX_FILE_SYSTEMS", "console": "OS_MAX_CONSOLES",
              "condvar": "OS_MAX_CONDVARS"}
    sizes = {"task": "OS_task_table", "queue": "OS_queue_table", "countsem": "OS_count_sem_table",
             "binsem": "OS_bin_sem_table", "mutex": "OS_mutex_table", "stream": "OS_stream_table",
             "dir": "OS_dir_table", "timebase": "OS_timebase_table", "timecb": "OS_timecb_table",
             "module": "OS_module_table", "filesys": "OS_filesys_table", "console": "OS_console_table",
             "condvar": "OS_condvar_table"}
    for t in sorted(limits):
        try:
            lim = int(ev("sizeof(%s)/sizeof(%s[0])" % (sizes[t], sizes[t])))
        except gdb.error:
            lim = -1
        print("MEM OSAL %s %d %d" % (t, counts.get(t, 0), lim))
    for i, q in enumerate(iter_array("OS_queue_table")):
        if i in active_queues:
            print("MEM QUEUE %d %d %d" % (i, int(q["max_depth"]), int(q["max_size"])))


def report_psp():
    print("MEM PSP ReservedUsed %d" % int(ev("CFE_PSP_FreeRTOS_ReservedUsed")))
    print("MEM PSP ReservedBytes %d" % int(ev("sizeof(CFE_PSP_FreeRTOS_ReservedMem)")))
    m = ev("CFE_PSP_ReservedMemoryMap")
    for f in ("ResetMemory", "VolatileDiskMemory", "CDSMemory", "UserReservedMemory"):
        print("MEM PSP %s %d" % (f, int(m[f]["BlockSize"])))


for part in (report_stacks, report_cfe, report_osal, report_psp):
    try:
        part()
    except gdb.error as e:
        print("MEM ERROR %s: %s" % (part.__name__, e))
print("MEM END")
