#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <signal.h>
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

void sighup() 
{ 
    signal(SIGHUP, sighup); /* reset signal */
    printf("CHILD: I have received a SIGHUP\n"); 
}

int main(int argc, char *argv[]) 
{   
    if (parse_options(argc, argv) < 0)
		return usage(argv[0]);

	ssize_t nread;
	char* name;
	char** dataset = initializer();
    char** mydata = empty_allocator();
	char** hm = empty_allocator();

	int pip[2], child_pipe_first[2], child_pipe_second[2], fd[2];
	if (pipe(child_pipe_second) < 0 || pipe(child_pipe_first) < 0 || pipe(pip) < 0 || pipe(fd) < 0) 
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
		pid_t second_child;
		pid_t mypid = getpid();

		write(child_pipe_first[1], &mypid, sizeof(pid_t));
		read(child_pipe_second[0], &second_child, sizeof(pid_t));
		// mydata[0] = "Start";
		write(pip[1], &mydata, sizeof(char **));

		start = clocker(0, name);
		signal(SIGHUP, sighup);
		pause();
		end = clocker(1, name);

        write(pip[1], &start, sizeof(clock_t));
        write(pip[1], &end, sizeof(clock_t));
		// signal(SIGHUP, sighup);
		// pause();
		exit(0);
	}
	else
	{
		pid_t   child_2_pid;
		if((child_2_pid = fork()) == -1)
		{
			perror("fork");
			exit(1);
		}
		if(child_2_pid == 0)
		{
        	name = "Second Child"; 
			
			pid_t first_child;
			read(child_pipe_first[0], &first_child, sizeof(pid_t));
        	
			pid_t mypid = getpid();
			write(child_pipe_second[1], &mypid, sizeof(pid_t));

			char **rdata;
			read(pip[0], &rdata, sizeof(char**));

			start = clocker(0, name);
			nread = do_vm_rwv(first_child, dataset, rdata, 1);
			kill(first_child, SIGHUP);

			// size_t nread2 = do_vm_rwv(first_child, hm, rdata, 0);
			// kill(first_child, SIGHUP);


			clock_t sstart;
			read(pip[0], &sstart, sizeof(clock_t));
			read(pip[0], &end, sizeof(clock_t));
			printf("---------------------------------------------\n");
			// printf("[%s,\n%s],\n\n[%s,\n%s].\n ",dataset[0], hm[0], dataset[5], hm[5]);
			double first_result = time_calc(end, start, name);
			printf("The frequency is eqals to(Mbps): %f .\n", (double)((K_MULTIPLY*SPLICE_SIZE)/(first_result*meg)));
        	
			exit(0);
		}
		else
		{
			/* Parent process closes up output side of pipe */
        	name = "Parent"; 
			int child1_status, child2_status;
			waitpid(childpid, &child1_status, 0);
			waitpid(child_2_pid, &child2_status, 0);

			if (child1_status == 0 && child2_status == 0)  // Verify child process terminated without error. 
			{
				printf("The child process terminated normally.\n");    
			}
			else{printf("The child process terminated with an error!.\n");}
	        
			exit(0);
		}

	}
    return 0;
}
