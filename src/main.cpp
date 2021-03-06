#include "graph.h"

#include <cstring>
#include <fstream>
#include <iostream>

static void usage();

int main(int argc, char **argv)
{
	try {
		Graph g;
		OutputFormat format = OutputFormat::DeadlockFlow;
		// TODO: tiemscape should be determined from the input
		double timescale = 1.0 / 10'000; // 1px = 100us
		if (argc <= 1) {
			usage();
			throw std::runtime_error("Too few arguments");
		}
		for (int i = 1; i < argc; ++ i) {
			if (argv[i] == nullptr ||
			    strcmp(argv[i], "-h") == 0 ||
			    strcmp(argv[i], "--help") == 0)
			{
				usage();
				return EXIT_SUCCESS;
			} else if (strcmp(argv[i], "-d") == 0) {
				format = OutputFormat::GraphvizDOT;
			} else if (strcmp(argv[i], "-t") == 0) {
				if (i == argc - 1) {
					usage();
					throw std::runtime_error("Missing timescale value");
				}
				timescale = 1.0 / std::stoi(argv[i+1]);
				++ i;
			} else if (strcmp(argv[i], "--") == 0) {
				if (i != argc - 1) {
					usage();
					throw std::runtime_error("Too many arguments");
				}
				g = Graph{std::cin};
			} else {
				if (i != argc - 1) {
					usage();
					throw std::runtime_error("Too many arguments");
				}
				std::ifstream file;
				file.open(argv[i], std::fstream::in);
				if (!file) {
					throw std::runtime_error("Could not open file: " + std::string{argv[i]});
				}
				file.exceptions(std::fstream::failbit);
				g = Graph{file};
			}
		}
		if (format == OutputFormat::GraphvizDOT) {
			dump_dot(g);
		} else {
			dump_flow(g, timescale);
		}
	} catch (const std::ifstream::failure &) {
		std::cerr << "Error: Could not parse file" << std::endl;
	} catch (const std::exception &e) {
		std::cerr << "Error: " << e.what() << std::endl;
	}
	return EXIT_SUCCESS;
}

static void usage()
{
	std::cerr << "Usage: deadlock-graph [-d] [--|file]\n"
	          << "Options:\n"
	          << "  --         Read from stdin.\n"
	          << "  -d         Output in Graphviz DOT format.\n"
	          << "  -t [ns]    Timescale, number of nanoseconds per pixel in flow graph\n"
	          << "  -h --help  Display this information.\n"
	          << std::endl;
}
