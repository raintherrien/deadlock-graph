#ifndef DEADLOCK_GRAPH_GRAPH_H_
#define DEADLOCK_GRAPH_GRAPH_H_

#include <istream>
#include <limits>
#include <string>
#include <vector>

enum class OutputFormat {
	GraphvizDOT,
	DeadlockFlow
};

struct NodeDescription {
	std::string file;
	std::string func;
	unsigned long line;
};

struct Edge {
	unsigned long long ts_ns;
	unsigned long head;
	unsigned long tail;
};

struct Node {
	std::string label;
	unsigned long long begin_ns;
	unsigned long long end_ns;
	unsigned long thread;
	unsigned long task;
	unsigned long description;
};

struct Graph {
	std::vector<NodeDescription> node_descriptions;
	std::vector<Edge> edges;
	std::vector<Node> nodes;
	unsigned long long begin_ns = std::numeric_limits<unsigned long long>::max();
	unsigned long long end_ns   = std::numeric_limits<unsigned long long>::lowest();
	unsigned long num_threads = 0;

	Graph() = default;
	Graph(std::istream &);
};

void dump_dot(const Graph &);
void dump_flow(const Graph &, double timescale);

#endif // DEADLOCK_GRAPH_GRAPH_H_
