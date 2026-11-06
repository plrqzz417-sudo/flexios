#pragma once
#include <sys/types.h>
#include <sys/uio.h>
#include <stdint.h>

// Used by the unchanged gameplay core. The pseudo syscall IDs are private to
// this adapter and are never sent to the iOS kernel as system call numbers.
#define SYS_gettid 0x4a4d0001
#define SYS_process_vm_readv 0x4a4d0002
#define syscall jm_ios_syscall

extern "C" long jm_ios_syscall(long operation,...);
void* jm_ios_unity_handle();
bool jm_ios_unity_address(const void* address);
