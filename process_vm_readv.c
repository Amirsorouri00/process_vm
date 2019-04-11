#define _GNU_SOURCE
#include <sys/uio.h>
#include <stdio.h>

/* 
The following code sample demonstrates the use of 
process_vm_readv(). It reads 20 bytes at the address 0x10000 
from the process with PID 10 and writes the first 10 bytes 
into buf1 and the second 10 bytes into buf2.
*/

int main(void) { 
    struct iovec local[2]; 
    struct iovec remote[1]; 
    char buf1[10] = "0123456789"; 
    char buf2[10] = "0123456789"; 
    ssize_t nread; 
    pid_t pid = 10;             /* PID of remote process */


    local[0].iov_base = buf1; 
    local[0].iov_len = 10; 
    local[1].iov_base = buf2; 
    local[1].iov_len = 10; 
    remote[0].iov_base = (void *) 0x10000; 
    remote[0].iov_len = 20;


    nread = process_vm_readv(pid, local, 2, remote, 1, 0); 
    if (nread != 20) {
        printf("here0;\n nread = %ld\n", nread);
        return 1; 
    }
    else {
        printf("here1;\n nread = %ld\n", nread);
        return 0; 
    }
}  