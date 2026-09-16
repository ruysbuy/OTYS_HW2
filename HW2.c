#define _GNU_SOURCE
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

static void SigfpeHandler(int signo, siginfo_t *info, void *ucontext)
{
    (void)signo; (void)info;
    ucontext_t *uc = (ucontext_t *)ucontext;
    uint8_t *rip = (uint8_t *)uc->uc_mcontext.gregs[REG_RIP];

    char buf[64];
    int n = snprintf(buf, sizeof buf, "RIP = %p\n", (void *)rip);
    write(STDERR_FILENO, buf, n);

    if (rip[0] == 0xf7 && rip[1] == 0xf9) {
        uc->uc_mcontext.gregs[REG_RAX] = 0;
        uc->uc_mcontext.gregs[REG_RDX] = 0;
        uc->uc_mcontext.gregs[REG_RIP] += 2;
    } else if (rip[0] == 0x41 && rip[1] == 0xf7 && rip[2] == 0xfc){
        uc->uc_mcontext.gregs[REG_RAX] = 0;
        uc->uc_mcontext.gregs[REG_RDX] = 0;
        uc->uc_mcontext.gregs[REG_RIP] += 3;
    } else if (rip[0] == 0xf7 && rip[1] == 0xf9){
        uc->uc_mcontext.gregs[REG_RAX] = 0;
        uc->uc_mcontext.gregs[REG_RDX] = 0;
        uc->uc_mcontext.gregs[REG_RIP] += 3;
    } else if (rip[0] == 0xf7 && rip[1] == 0x3c && rip[2] == 0x24){
        uc->uc_mcontext.gregs[REG_RAX] = 0;
        uc->uc_mcontext.gregs[REG_RDX] = 0;
        uc->uc_mcontext.gregs[REG_RIP] += 3;
    } else {
        _exit(EXIT_FAILURE);
    }

}

static void InstallHandler(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_sigaction = SigfpeHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGFPE, &sa, NULL);
}

// /* ---------- Сценарий A ---------- */
static void ScenarioA(void)
{
    write(STDERR_FILENO, "=== A === \n", 9);

    volatile int a = 100;
    volatile int b = 0;

    int c = a / b;

    printf("[A] after division: c = %d\n", c);

    if (c == 0 || b == 0)
        write(STDERR_FILENO, "b is 0\n", 7);
    else
        write(STDERR_FILENO, "b is NOT 0\n", 11);    
}

// /* ---------- Сценарий B ---------- */
static void ScenarioB(void)
{
    write(STDERR_FILENO, "=== B ===\n", 9); 

    volatile int a = 1;
    volatile int b_vol = 0;

    int b = b_vol;

    volatile int c = a / b;

    printf("[A] after division: c = %d\n", c);

    if (c == 0 || b == 0)
        write(STDERR_FILENO, "b is 0\n", 7);
    else
        write(STDERR_FILENO, "b is NOT 0\n", 11);
}

int main(void)
{
    InstallHandler();

    ScenarioA();
    ScenarioB();

    return 0;
}