/* ptrace-launch.c — m2s-cfs debug helper: run a program so any gdb may attach later
 * (needed because kernel.yama.ptrace_scope=1). Build: gcc -O2 -o ptrace-launch ptrace-launch.c
 * Use: ./ptrace-launch ./sem-speed-test &   then after a hang: gdb -p <pid> */
#include <sys/prctl.h>
#include <unistd.h>
#include <stdio.h>
int main(int c, char **v) { (void)c; prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0); execv(v[1], &v[1]); perror("execv"); return 127; }
