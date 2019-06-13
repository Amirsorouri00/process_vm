#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "process-vm-benchmarking.h"


#define meg     1000000
/* 
    The following code sample demonstrates the use of 
    process_vm_readv().
*/

static int align_mask = 65535;
static int splice_flags;
clock_t start, end;

static int parse_options(int argc, char *argv[])
{
	int opt, index = 1;

	while ((opt = getopt(argc, argv, ":gu")) != -1) {
		switch (opt) {
			case 'u':
				splice_flags = SPLICE_F_MOVE;
				index++;
				break;
			case 'g':
				printf("gift\n");
				splice_flags = SPLICE_F_GIFT;
				index++;
				break;
			default:
				return -1;
		}
	}
    for(; optind <= argc; optind++){      
        printf("extra arguments: %s\n", argv[optind-1]);  
    } 

	return index;
}

int do_vm_rwv(pid_t proc, char **localdata, char** remotedata, int readwrite)
{
    printf("here");
    int page_counter = K_MULTIPLY - 1;
	struct iovec localiov[] = {
		{
			.iov_base = localdata[page_counter],
			.iov_len = SPLICE_SIZE,
		},
	};
    struct iovec remoteiov[] = {
		{
			.iov_base = remotedata[page_counter],
			.iov_len = SPLICE_SIZE,
		},
	};
	int written, idx = 0, nread = 0;
	while (page_counter >= 0) 
    {
        if(0 == readwrite)
        {
            written = sreadv(proc, localiov, 1, remoteiov, 1, 0);
            if (written <= 0)
			    return error("readv");
        }
        else if (1 == readwrite)
        {
            written = swritev(proc, localiov, 1, remoteiov, 1, 0);
            if (written <= 0)
			    return error("writev");
        }
		if ((size_t) written >= SPLICE_SIZE) {
            nread+=written;
			page_counter--;
			localiov[idx].iov_len = SPLICE_SIZE;
			localiov[idx].iov_base = localdata[page_counter];
			remoteiov[idx].iov_len = SPLICE_SIZE;
            remoteiov[idx].iov_base = remotedata[page_counter];
		} else {
            nread+=written;
			localiov[idx].iov_len -= written;
			localiov[idx].iov_base += written;
            remoteiov[idx].iov_len -= written;
			remoteiov[idx].iov_base += written;
		}
	}

    if(nread < 0){
        return -1;
    }
	return nread;
}

int main(int argc, char *argv[]) 
{   
    if (parse_options(argc, argv) < 0)
		return usage(argv[0]);

	ssize_t nread;
	char* name;
	int pip[2], child_pipe[2], fd[2];
	if (pipe(child_pipe) < 0 || pipe(pip) < 0 || pipe(fd) < 0) 
		exit(1);

	pid_t   childpid;
	if((childpid = fork()) == -1)
	{
		perror("fork");
		exit(1);
	}
	if(childpid == 0)
	{
		/* First Child process closes up input side of pipe. */
		name = "First Child";
		close(pip[0]);
		close(child_pipe[0]);
        pid_t mypid = getpid();
		write(child_pipe[1], &mypid, sizeof(mypid));

        char** mydata = empty_allocator();
        write(pip[1], &mydata, sizeof(char**));

        char *endfs = NULL;
        read(fd[0], &endfs, sizeof(char**));
        printf("%s", endfs);

		exit(0);
	}
	else
	{
        /* Parent process closes up output side of pipe */
        name = "Parent";
       
        pid_t childpid;
        char **rdata;
		read(child_pipe[0], &childpid, sizeof(childpid));
		read(pip[0], &rdata, sizeof(char**));
        
		printf("---------------------------------------------\n");
        char** dataset = initializer();
        char** hm = empty_allocator();

        start = clocker(0, name);
		nread = do_vm_rwv(childpid, dataset, rdata, 1);
		nread = do_vm_rwv(childpid, hm, rdata, 0);
		end = clocker(1, name);
        printf("[%s,\n%s],\n\n [%s,\n%s].\n ",dataset[0], hm[0], dataset[5], hm[5]);
		printf("---------------------------------------------\n");
        double first_result = time_calc(end, start, name);

        printf("The frequency is eqals to(Mbps): %f .\n", (double)((2* K_MULTIPLY*SPLICE_SIZE)/(first_result*meg)));

        char* endfs = "done.";
		write(fd[1], &endfs, sizeof(char**));

        int child1_status;
        waitpid(childpid, &child1_status, 0);

        if (child1_status == 0)  // Verify child process terminated without error. 
        {
            printf("The child process terminated normally.\n");    
        }
        else{printf("The child process terminated with an error!.\n");}

        exit(0);
	}
    return 0;
}
