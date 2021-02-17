#include "graph.h"

#include <sstream>

Graph::Graph(std::istream &f)
{
	std::size_t line;
	std::stringstream ls;

	auto slurp_line = [&]() {
		std::string l;
		++ line;
		std::getline(f, l);
		ls = std::stringstream{l};
	};

	auto expect = [&](const std::string &expect) {
		std::string rest;
		std::getline(ls, rest);
		if (rest == expect) {
			return;
		}
		throw std::runtime_error("Expected \"" + expect	+ "\" on line " + std::to_string(line) + " but found \"" + rest + "\"");
	};

	// Read number of node descriptions
	slurp_line();
	std::size_t node_description_count;
	ls >> node_description_count;
	expect(" node descriptions");

	// Read node descriptions
	for (std::size_t i = 0; i < node_description_count; ++ i) {
		NodeDescription d;

		slurp_line();
		std::getline(ls, d.file);

		slurp_line();
		ls >> d.line;
		expect("");

		slurp_line();
		std::getline(ls, d.func);
		node_descriptions.emplace_back(d);
	}

	// Read number of edges
	slurp_line();
	std::size_t edge_count;
	ls >> edge_count;
	expect(" edges");

	// Read edges, sort by head
	for (std::size_t i = 0; i < edge_count; ++ i) {
		Edge e;

		slurp_line();
		ls >> e.ts_ns;
		ls >> e.head;
		ls >> e.tail;
		expect("");

		edges.emplace_back(e);
	}
	std::sort(std::begin(edges), std::end(edges),
	          [](const auto &a, const auto &b) { return a.head < b.head; });

	// Read number of nodes
	slurp_line();
	std::size_t node_count;
	ls >> node_count;
	expect(" nodes");

	// Read nodes and max thread
	for (std::size_t i = 0; i < node_count; ++ i) {
		Node n;

		slurp_line();
		std::getline(ls, n.label);
		slurp_line();
		ls >> n.thread;
		ls >> n.task;
		ls >> n.description;
		ls >> n.begin_ns;
		ls >> n.end_ns;
		expect("");

		if (n.label == "(null)") {
			n.label = node_descriptions.at(n.description).func;
		}

		if (n.thread+1 > num_threads)
			num_threads = n.thread+1;
		if (n.begin_ns < begin_ns)
			begin_ns = n.begin_ns;
		if (n.end_ns > end_ns)
			end_ns = n.end_ns;

		nodes.emplace_back(n);
	}
}
