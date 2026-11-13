/*
 * Step 12a phase 1 (m2s-cfs): how does a task of the FreeRTOS POSIX port behave while it waits in a host
 * socket call? Host-side test program, no cFE and no OSAL.
 *
 * Tasks: "low" (priority 1) counts in a busy loop; its count per second measures how much CPU it gets.
 * "waiter" (priority 3) runs four scenarios on a UDP socket bound to 127.0.0.1, each WAIT_MS long:
 *   A  blocking:  one host select() with a WAIT_MS timeout, repeated after EINTR until WAIT_MS has passed;
 *   B  polling:   select() with a zero timeout, then vTaskDelay(1 tick), until WAIT_MS has passed;
 *   C  blocking as A, with a datagram sent by a forked child process DATA_AFTER_MS after the start;
 *   D  polling as B, with the same datagram.
 * Per scenario it prints the low task's count per second (and relative to a baseline second with the waiter
 * delayed), select() calls and EINTR returns, the waiter thread's CPU time per second, and for C/D the
 * receive latency (host CLOCK_MONOTONIC; the child puts its send time in the datagram).
 *
 * Exit status: 0 = all scenarios ran (the results are measurements, judged by the reader); 1 = setup error.
 */
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include "FreeRTOS.h"
#include "task.h"

#define WAIT_MS       3000
#define DATA_AFTER_MS 1000
#define PORT          47123

static StaticTask_t xLowTcb;
static StackType_t  xLowStack[configMINIMAL_STACK_SIZE * 4];
static StaticTask_t xWaiterTcb;
static StackType_t  xWaiterStack[configMINIMAL_STACK_SIZE * 8];

static volatile unsigned long long ullLowCount;
static int                         iExit = 1;

void vAssertCalled(const char *pcFile, unsigned long ulLine)
{
    fprintf(stderr, "ASSERT FAILED: %s:%lu\n", pcFile, ulLine);
    exit(2);
}

static long long llMonoUs(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (long long)t.tv_sec * 1000000LL + t.tv_nsec / 1000LL;
}

static long long llThreadCpuUs(void)
{
    struct timespec t;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t);
    return (long long)t.tv_sec * 1000000LL + t.tv_nsec / 1000LL;
}

static void vLowTask(void *pv)
{
    (void)pv;
    for (;;)
    {
        ++ullLowCount;
    }
}

/* Child process: sleep, send one datagram holding the send time, exit. Signals blocked (no FreeRTOS here). */
static void vForkSender(void)
{
    pid_t pid = fork();
    if (pid == 0)
    {
        sigset_t all;
        int      s = socket(AF_INET, SOCK_DGRAM, 0);
        struct sockaddr_in to;
        long long          t;

        sigfillset(&all);
        sigprocmask(SIG_BLOCK, &all, NULL);
        usleep(DATA_AFTER_MS * 1000);
        memset(&to, 0, sizeof(to));
        to.sin_family      = AF_INET;
        to.sin_port        = htons(PORT);
        to.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        t                  = llMonoUs();
        (void)sendto(s, &t, sizeof(t), 0, (struct sockaddr *)&to, sizeof(to));
        _exit(0);
    }
}

typedef struct
{
    unsigned long long low;
    long long          wall_us, cpu_us, latency_us;
    int                selects, eintr, ready, last_errno;
} Result_t;

/* Waits on fd for WAIT_MS; blocking = one long select() (repeated after EINTR), else zero-timeout polling. */
static void vWait(int fd, int blocking, Result_t *r)
{
    long long          start = llMonoUs(), end = start + WAIT_MS * 1000LL, cpu0 = llThreadCpuUs();
    unsigned long long low0  = ullLowCount;

    memset(r, 0, sizeof(*r));
    r->latency_us = -1;
    while (llMonoUs() < end)
    {
        fd_set         rd;
        struct timeval tv;
        long long      left = end - llMonoUs();
        int            rc;

        FD_ZERO(&rd);
        FD_SET(fd, &rd);
        tv.tv_sec  = blocking ? (time_t)(left / 1000000LL) : 0;
        tv.tv_usec = blocking ? (suseconds_t)(left % 1000000LL) : 0;
        if (left <= 0)
        {
            break;
        }
        rc = select(fd + 1, &rd, NULL, NULL, &tv);
        ++r->selects;
        if (rc < 0)
        {
            r->last_errno = errno;
            if (errno == EINTR)
            {
                ++r->eintr;
                continue;
            }
            break;
        }
        if (rc > 0 && r->latency_us < 0)
        {
            long long sent = 0;
            long long now  = llMonoUs();
            if (recv(fd, &sent, sizeof(sent), 0) == (ssize_t)sizeof(sent))
            {
                r->latency_us = now - sent;
            }
            ++r->ready;
        }
        if (!blocking)
        {
            vTaskDelay(1);
        }
    }
    r->wall_us = llMonoUs() - start;
    r->cpu_us  = llThreadCpuUs() - cpu0;
    r->low     = ullLowCount - low0;
}

static void vWaiterTask(void *pv)
{
    static const struct { const char *name; int blocking, data; } sc[] = {
        {"A blocking select, no data", 1, 0},
        {"B polling select,  no data", 0, 0},
        {"C blocking select, data at 1 s", 1, 1},
        {"D polling select,  data at 1 s", 0, 1},
    };
    struct sockaddr_in a;
    unsigned long long base0;
    double             base_per_s;
    int                fd, i;
    Result_t           r;

    (void)pv;
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    memset(&a, 0, sizeof(a));
    a.sin_family      = AF_INET;
    a.sin_port        = htons(PORT);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (fd < 0 || bind(fd, (struct sockaddr *)&a, sizeof(a)) != 0)
    {
        printf("SETUP ERROR: socket/bind: %s\n", strerror(errno));
        fflush(stdout);
        vTaskEndScheduler();
    }

    /* Baseline: the waiter is blocked in FreeRTOS for 1 s; the low task should get the CPU. */
    base0 = ullLowCount;
    vTaskDelay(pdMS_TO_TICKS(1000));
    base_per_s = (double)(ullLowCount - base0);
    printf("BASELINE low task count per second = %.0f (waiter in vTaskDelay)\n", base_per_s);

    for (i = 0; i < (int)(sizeof(sc) / sizeof(sc[0])); ++i)
    {
        if (sc[i].data)
        {
            vForkSender();
        }
        vWait(fd, sc[i].blocking, &r);
        printf("SCENARIO %s: wall %lld ms, low task %.0f/s (%.1f %% of baseline), selects %d, EINTR %d, "
               "last errno %d, ready %d, waiter CPU %.1f ms/s, receive latency %s%lld us\n",
               sc[i].name, r.wall_us / 1000, (double)r.low * 1e6 / (double)r.wall_us,
               100.0 * ((double)r.low * 1e6 / (double)r.wall_us) / base_per_s, r.selects, r.eintr, r.last_errno,
               r.ready, (double)r.cpu_us * 1000.0 / (double)r.wall_us, r.latency_us < 0 ? "none " : "",
               r.latency_us < 0 ? 0LL : r.latency_us);
        fflush(stdout);
        while (waitpid(-1, NULL, WNOHANG) > 0)
        {
        }
    }
    iExit = 0;
    vTaskEndScheduler();
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    (void)xTaskCreateStatic(vLowTask, "low", configMINIMAL_STACK_SIZE * 4, NULL, 1, xLowStack, &xLowTcb);
    (void)xTaskCreateStatic(vWaiterTask, "waiter", configMINIMAL_STACK_SIZE * 8, NULL, 3, xWaiterStack, &xWaiterTcb);
    vTaskStartScheduler();
    printf("RESULT: %s\n", iExit == 0 ? "DONE" : "SETUP ERROR");
    return iExit;
}
