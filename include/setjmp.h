#ifndef LIBC_SETJMP_H
#define LIBC_SETJMP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct __attribute__((aligned(16))) {
    uintptr_t rbx;
    uintptr_t rsp;
    uintptr_t rbp;
    uintptr_t rsi;
    uintptr_t rdi;
    uintptr_t r12;
    uintptr_t r13;
    uintptr_t r14;
    uintptr_t r15;
    uintptr_t rip;
    uint32_t mxcsr;
    uint16_t fp_control_word;
    uint16_t reserved0;
    uint32_t reserved1;
    uint32_t reserved2;
    struct {
        uint64_t low;
        uint64_t high;
    } xmm[10];
} __instant_jmp_buf;

typedef __instant_jmp_buf jmp_buf[1];

int setjmp(jmp_buf env);
__attribute__((noreturn)) void longjmp(jmp_buf env, int value);

#ifdef __cplusplus
}
#endif

#endif
