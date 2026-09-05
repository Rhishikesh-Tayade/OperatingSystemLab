#include <iostream>
#include <fstream>
#include <string>
#include <cstdlib>
#include <unistd.h>

using namespace std;

int main(int argc, char **argv) {
	if(argc != 5)
	{
		cout <<"usage: ./part1.out <path-to-file> <pattern> <search-start-position> <search-end-position>\nprovided arguments:\n";
		for(int i = 0; i < argc; i++)
			cout << argv[i] << "\n";
		return -1;
	}

	char *file_to_search_in = argv[1];
	char *pattern_to_search_for = argv[2];
	int search_start_position = atoi(argv[3]);
	int search_end_position = atoi(argv[4]);

	int length = search_end_position - search_start_position + 1;
	if (length <= 0)
	{
		cout << "[" << getpid() << "] didn't find\n";
		return 0;
	}

	ifstream file(file_to_search_in);
	if (!file)
	{
		cout << "[" << getpid() << "] didn't find\n";
		return 0;
	}
	file.seekg(search_start_position);

	string content(length, '\0');
	streamsize got = 0;
	file.read(&content[0], length);
	got = file.gcount();
	content.resize((size_t)got);

	size_t pos = content.find(pattern_to_search_for);
	if (pos != string::npos)
	{
		cout << "[" << getpid() << "] found at " << (search_start_position + pos) << "\n";
		return 1;
	}

	cout << "[" << getpid() << "] didn't find\n";
	return 0;
}