#include "Platform.h"
#undef syscall
#include <Foundation/Foundation.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach-o/dyld.h>
#include <pthread.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

extern "C" int __android_log_print(int,const char* tag,const char* format,...) {
    char buffer[1024];va_list args;va_start(args,format);
    int written=vsnprintf(buffer,sizeof(buffer),format,args);va_end(args);
    NSLog(@"[%s] %s",tag?tag:"JM",buffer);
    return written;
}

extern "C" long jm_ios_syscall(long operation,...) {
    va_list args;va_start(args,operation);
    if(operation==SYS_gettid){
        uint64_t tid=0;pthread_threadid_np(nullptr,&tid);va_end(args);return long(tid);
    }
    if(operation==SYS_process_vm_readv){
        const int pid=va_arg(args,int);
        auto local=va_arg(args,const iovec*);const int local_count=va_arg(args,int);
        auto remote=va_arg(args,const iovec*);const int remote_count=va_arg(args,int);
        (void)va_arg(args,int);va_end(args);
        if(pid!=getpid()||local_count!=1||remote_count!=1||!local||!remote||
           !local[0].iov_base||!remote[0].iov_base||local[0].iov_len<remote[0].iov_len)return -1;
        mach_vm_size_t transferred=0;
        const auto result=mach_vm_read_overwrite(mach_task_self(),
            mach_vm_address_t(remote[0].iov_base),remote[0].iov_len,
            mach_vm_address_t(local[0].iov_base),&transferred);
        return result==KERN_SUCCESS?long(transferred):-1;
    }
    va_end(args);return -1;
}

void* jm_ios_unity_handle(){
    for(uint32_t i=0;i<_dyld_image_count();++i){
        const char* path=_dyld_get_image_name(i);
        if(path&&strstr(path,"/UnityFramework.framework/UnityFramework"))
            return dlopen(path,RTLD_NOW|RTLD_NOLOAD);
    }
    return nullptr;
}
bool jm_ios_unity_address(const void* address){
    Dl_info image{};
    return address&&dladdr(address,&image)&&image.dli_fname&&
           strstr(image.dli_fname,"/UnityFramework.framework/UnityFramework");
}
