#include "graph.h"

#include <cstdio>

// TODO: C++20 std::format, unsupported by current GCC/Clang
void dump_dot(const Graph &g)
{
	// Prioritize output file size over readability
	std::puts("digraph G{");
	for (const auto &node : g.nodes) {
		const auto &desc = g.node_descriptions[node.description];
		double ms = double(node.end_ns - node.begin_ns) / 1'000'000;
		std::printf(R"(%lu[tooltip="%s:%lu: %s %fms";label="%s"];)",
		            node.task, desc.file.c_str(), desc.line,
		            desc.func.c_str(), ms, node.label.c_str());
	}
	for (const auto &edge : g.edges) {
		std::printf("%lu->%lu;", edge.head, edge.tail);
	}
	std::puts("}\n");
}
