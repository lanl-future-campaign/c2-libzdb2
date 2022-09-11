#include <errno.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

/*
 * Example. ./zdb_pp [dev#] [pool_name]
 */
int
main(int argc, char *argv[])
{
	const char *poolname = "pool";
	int n = 5;
	if (argc > 1) {
		n = atoi(argv[1]);
		if (n <= 0) {
			fprintf(stderr, "Invalid dev count: '%s'\n", argv[1]);
			return -1;
		}
	}
	if (argc > 2) {
		poolname = argv[2];
	}
	std::vector<FILE *> map;
	map.resize(n);
	char tmp[100];
	for (int i = 0; i < map.size(); i++) {
		snprintf(tmp, sizeof(tmp), "%s-%d", poolname, i);
		map[i] = fopen(tmp, "w");
		if (!map[i]) {
			fprintf(stderr, "Cannot open file %s: %s\n", tmp,
			    strerror(errno));
			return -1;
		}
	}
	std::string line;
	while (std::cin >> line) {
		if (line[0] != '#') {
			break;
		}
		std::string::size_type d1 = line.find_first_of(',', 1);
		int i = atoi(line.substr(1, d1 - 1).c_str());
		std::string::size_type d2 = line.find_first_of(',', d1 + 1);
		long long int offset =
		    atoll(line.substr(d1 + 1, d2 - d1 - 1).c_str());
		offset += 1 << 20;
		fputs("/dev/sda4:", map[i]);
		char tmp[50];
		sprintf(tmp, "%lld", offset);
		fputs(tmp, map[i]);
		fputs(":1048576\n", map[i]);
	}
	for (int i = 0; i < map.size(); i++) {
		fflush(map[i]);
		fclose(map[i]);
	}
	return 0;
}
