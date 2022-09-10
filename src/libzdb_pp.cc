#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <vector>

static std::vector<std::string> map;

int
main(int argc, char *argv[])
{
	map.resize(5);
	for (int i = 0; i < map.size(); i++) {
		map[i].reserve(4096);
	}
	std::string line;
	while (std::cin >> line) {
		if (line[0] != '#') {
			break;
		}
		std::string::size_type d1 = line.find_first_of(',', 1);
		int devidx = atoi(line.substr(1, d1 - 1).c_str());
		std::string::size_type d2 = line.find_first_of(',', d1 + 1);
		std::string offset = line.substr(d1 + 1, d2 - d1 - 1);
		map[devidx].append(offset);
		map[devidx].append(",");
	}
	for (int i = 0; i < map.size(); i++) {
		std::cout << i << ":" << map[i].c_str() << std::endl;
	}
	return 0;
}
