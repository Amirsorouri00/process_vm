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
	int pip[2], child_pipe[2];
	if (pipe(child_pipe) < 0 || pipe(pip) < 0) 
		exit(1);

	char** data1 = empty_allocator();
	char** data = initializer();

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
		close(child_pipe[1]);
        pid_t second_child;
        read(child_pipe[0], &second_child, sizeof(start));

		start = clocker(0, name);   
		nread = do_vm_rwv(second_child, data, data1, 1);
		end = clocker(1, name);

		printf("---------------------------------------------\n");
        double first_result = time_calc(start, end, name);
		write(pip[1], &start, sizeof(start));
		exit(0);
	}
	else
	{
        pid_t child2_pid;
        if((child2_pid = fork()) == -1)
        {
            error("fork");
            exit(1);
        }
        if(child2_pid == 0)
	    {
            /* Second Child process closes up output side of pipe */
            name = "Second Child";
            close(pip[1]);             
            close(child_pipe[0]);
    		write(child_pipe[1], &child2_pid, sizeof(start));
            char** datas = empty_allocator();
            
            start = clocker(0, name);
            nread = do_vm_rwv(childpid, data1, datas, 1);
            end = clocker(1, name);

            printf("in %s: number of reads from the pipe = %ld\n", name, nread);
            printf("---------------------------------------------\n");
            clock_t first_start;
            read(pip[0], &first_start, sizeof(start));
            // read(pip[0], &first_end, sizeof(start));
			// double first_result = time_calc(first_end, first_start, "First Child");
            printf("---------------------------------------------\n");
			double result = time_calc(end, start, name);
            printf("---------------------------------------------\n");
            result = time_calc(end, first_start, "realtime_calculation");
            // printf("time to write and read into the pipe by vmsplice = %f\n", result);
			exit(0);
        }
        else{
            /* Parent process closes up output side of pipe */
            int child1_status, child2_status;
            waitpid(childpid, &child1_status, 0);
            waitpid(child2_pid, &child2_status, 0);
            if (child1_status == 0 && child2_status == 0)  // Verify child process terminated without error.  
            {
                printf("The child processes terminated normally.\n");    
            }
            else{printf("The child process terminated with an error!.\n");}
			free_allocator(data);
            // free_allocator(data1);
            exit(0);
        }
	}
    return 0;
}
