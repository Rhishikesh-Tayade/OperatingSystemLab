#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>
#include <cstdlib>
#include <cerrno>

using namespace std;

// Global variables to track active child processes
pid_t left_child_pid = 0;
pid_t right_child_pid = 0;
pid_t searcher_child_pid = 0;

void sigterm_handler(int sig)
{
	cout << "[" << getpid() << "] received SIGTERM\n";
	
	// Terminate active children
	if (left_child_pid > 0)
	{
		kill(left_child_pid, SIGTERM);
	}
	if (right_child_pid > 0)
	{
		kill(right_child_pid, SIGTERM);
	}
	if (searcher_child_pid > 0)
	{
		kill(searcher_child_pid, SIGTERM);
	}

	// Wait for children to clean up and exit
	int status;
	if (left_child_pid > 0)
	{
		waitpid(left_child_pid, &status, 0);
	}
	if (right_child_pid > 0)
	{
		waitpid(right_child_pid, &status, 0);
	}
	if (searcher_child_pid > 0)
	{
		waitpid(searcher_child_pid, &status, 0);
	}

	_exit(0);
}

int main(int argc, char **argv)
{
	// Register SIGTERM handler
	signal(SIGTERM, sigterm_handler);

	if(argc != 6)
	{
		cout <<"usage: ./part3_partitioner.out <path-to-file> <pattern> <search-start-position> <search-end-position> <max-chunk-size>\nprovided arguments:\n";
		for(int i = 0; i < argc; i++)
			cout << argv[i] << "\n";
		return -1;
	}
	
	char *file_to_search_in = argv[1];
	char *pattern_to_search_for = argv[2];
	int search_start_position = atoi(argv[3]);
	int search_end_position = atoi(argv[4]);
	int max_chunk_size = atoi(argv[5]);
	
	int my_pid = getpid();
	cout << "[" << my_pid << "] start position = " << search_start_position << " ; end position = " << search_end_position << "\n";

	if (search_end_position - search_start_position + 1 > max_chunk_size)
	{
		int mid = (search_start_position + search_end_position) / 2;

		pid_t left_pid = fork();
		if (left_pid < 0)
		{
			perror("fork");
			return -1;
		}
		if (left_pid == 0)
		{
			// Child runs the same executable
			execlp(argv[0], argv[0], file_to_search_in, pattern_to_search_for, to_string(search_start_position).c_str(), to_string(mid).c_str(), to_string(max_chunk_size).c_str(), nullptr);
			exit(1);
		}
		left_child_pid = left_pid;
		cout << "[" << my_pid << "] forked left child " << left_pid << "\n";

		pid_t right_pid = fork();
		if (right_pid < 0)
		{
			perror("fork");
			return -1;
		}
		if (right_pid == 0)
		{
			// Child runs the same executable
			execlp(argv[0], argv[0], file_to_search_in, pattern_to_search_for, to_string(mid + 1).c_str(), to_string(search_end_position).c_str(), to_string(max_chunk_size).c_str(), nullptr);
			exit(1);
		}
		right_child_pid = right_pid;
		cout << "[" << my_pid << "] forked right child " << right_pid << "\n";

		int status = 0;
		int left_exit = 0, right_exit = 0;
		int reaped = 0;
		while (reaped < 2)
		{
			pid_t returned_pid = wait(&status);
			if (returned_pid < 0)
			{
				if (errno == ECHILD) break;
				continue;
			}
			reaped++;
			int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
			if (returned_pid == left_pid)
			{
				cout << "[" << my_pid << "] left child returned\n";
				left_exit = exit_code;
				left_child_pid = 0; // Completed
				if (left_exit == 1)
				{
					if (right_child_pid > 0)
					{
						kill(right_child_pid, SIGTERM);
						int temp_status;
						waitpid(right_child_pid, &temp_status, 0);
					}
					return 1;
				}
			}
			else if (returned_pid == right_pid)
			{
				cout << "[" << my_pid << "] right child returned\n";
				right_exit = exit_code;
				right_child_pid = 0; // Completed
				if (right_exit == 1)
				{
					if (left_child_pid > 0)
					{
						kill(left_child_pid, SIGTERM);
						int temp_status;
						waitpid(left_child_pid, &temp_status, 0);
					}
					return 1;
				}
			}
		}
		return (left_exit == 1 || right_exit == 1) ? 1 : 0;
	}
	else
	{
		pid_t searcher_pid = fork();
		if (searcher_pid < 0)
		{
			perror("fork");
			return -1;
		}
		if (searcher_pid == 0)
		{
			execlp("./part3_searcher.out", "./part3_searcher.out", file_to_search_in, pattern_to_search_for, to_string(search_start_position).c_str(), to_string(search_end_position).c_str(), nullptr);
			exit(1);
		}
		searcher_child_pid = searcher_pid;
		cout << "[" << my_pid << "] forked searcher child " << searcher_pid << "\n";

		int status = 0;
		waitpid(searcher_pid, &status, 0);
		cout << "[" << my_pid << "] searcher child returned\n";
		searcher_child_pid = 0;
		return WIFEXITED(status) ? WEXITSTATUS(status) : 0;
	}
}
