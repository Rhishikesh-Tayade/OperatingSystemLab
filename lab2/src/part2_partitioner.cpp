#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <string>

using namespace std;

int main(int argc, char **argv)
{
	if(argc != 6)
	{
		cout <<"usage: ./partitioner.out <path-to-file> <pattern> <search-start-position> <search-end-position> <max-chunk-size>\nprovided arguments:\n";
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
		if (left_pid == 0)
		{
			execlp(argv[0], argv[0], file_to_search_in, pattern_to_search_for, to_string(search_start_position).c_str(), to_string(mid).c_str(), to_string(max_chunk_size).c_str(), NULL);
			exit(1);
		}
		cout << "[" << my_pid << "] forked left child " << left_pid << "\n";

		pid_t right_pid = fork();
		if (right_pid == 0)
		{
			execlp(argv[0], argv[0], file_to_search_in, pattern_to_search_for, to_string(mid + 1).c_str(), to_string(search_end_position).c_str(), to_string(max_chunk_size).c_str(), NULL);
			exit(1);
		}
		cout << "[" << my_pid << "] forked right child " << right_pid << "\n";

		int status;
		int left_exit = 0, right_exit = 0;
		for (int i = 0; i < 2; i++)
		{
			pid_t returned_pid = wait(&status);
			int exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : 0;
			if (returned_pid == left_pid)
			{
				cout << "[" << my_pid << "] left child returned\n";
				left_exit = exit_code;
			}
			else if (returned_pid == right_pid)
			{
				cout << "[" << my_pid << "] right child returned\n";
				right_exit = exit_code;
			}
		}
		return (left_exit == 1 || right_exit == 1) ? 1 : 0;
	}
	else
	{
		pid_t searcher_pid = fork();
		if (searcher_pid == 0)
		{
			execlp("./part2_searcher.out", "./part2_searcher.out", file_to_search_in, pattern_to_search_for, to_string(search_start_position).c_str(), to_string(search_end_position).c_str(), NULL);
			exit(1);
		}
		cout << "[" << my_pid << "] forked searcher child " << searcher_pid << "\n";

		int status;
		waitpid(searcher_pid, &status, 0);
		cout << "[" << my_pid << "] searcher child returned\n";
		return WIFEXITED(status) ? WEXITSTATUS(status) : 0;
	}
}
