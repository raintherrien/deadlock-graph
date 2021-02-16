#include "graph.h"

#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>

struct FlowNode {
	const Node &node;
	unsigned long task;
	float x, y, width;
};

static std::vector<FlowNode> flow_nodes(const Graph &g);

constexpr float header_height = 16;
constexpr float font_size = 8;
constexpr float node_height = 16;
constexpr float flow_node_text_vpadding = node_height / 2 + font_size / 4;
constexpr float thread_lane_height = 30;
constexpr float thread_lane_vpadding = (thread_lane_height - node_height) / 2;
constexpr float timescale = 1.0f / 1000.0f; // 1px = 1us

void
dump_flow(const Graph &g)
{
	const auto fnodes = flow_nodes(g);

	float time_delta = g.end_ns - g.begin_ns;
	float width  = time_delta * timescale;
	float height = header_height + g.num_threads * thread_lane_height;

	std::puts(R"(<?xml version="1.0" encoding="UTF-8" standalone="no"?>)");
	std::printf(R"(<svg viewBox="0 0 %f %f" xmlns="http://www.w3.org/2000/svg">)",
	            width, height);
	// styles
	std::printf("<style>"
	            "text{font-size:%fpx;text-anchor:middle;stroke:none}"
	            "path{fill:none;stroke-opacity:0.5}"
	            ".nodes rect{fill:lightgray}"
	            ".nodes text{fill:black}"
	            ".EG g use:nth-child(1n){stroke-width:5;pointer-events:stroke}"
	            ".EG g use:nth-child(2n){stroke-width:0.5;stroke:black}"
	            ".EG g:hover use:nth-child(2n){stroke:red;stroke-opacity:1;stroke-width:2}"
	            "</style>", font_size);
	// thread dividers
	std::puts(R"(<g stroke="gray" stroke-width="0.3" stroke-dasharray="2">)");
	for (unsigned long i = 0; i < g.num_threads+1; ++ i) {
		float y = header_height + i * thread_lane_height;
		std::printf(R"(<line x2="%f" y1="%f" y2="%f"/>)", width, y, y);
	}
	std::puts("</g>");
	// timestamps
	std::puts(R"(<g stroke="crimson" stroke-width="0.3">)");
	float timestamp_interval;
	const char *timestamp_header;
	if (time_delta > 1e9) {
		timestamp_interval = 1e9 / 60.0f; // every 60Hz (16.666ms)
		timestamp_header = "16.666ms";
	} else if (time_delta > 100e6) {
		timestamp_interval = 1e9 / 120.0f; // every 120Hz (8.333ms)
		timestamp_header = "8.333ms";
	} else if (time_delta > 1e6) {
		timestamp_interval = 100e3; // every 100us
		timestamp_header = "100μs";
	} else {
		timestamp_interval = 50e3; // every 50us
		timestamp_header = "50μs";
	}
	unsigned long long maxt = (g.end_ns - g.begin_ns) / timestamp_interval;
	for (unsigned long long t = 1; t < maxt; ++ t) {
		float x = t * timestamp_interval * timescale;
		std::printf(R"(<line x1="%f" x2="%f" y1="%f" y2="%f"/>)", x, x, header_height, height);
	}
	std::printf(R"(<text x="%f" y="%f">%s</text>)", timestamp_interval * timescale, header_height * .75, timestamp_header);
	std::puts("</g>");
	// nodes
	std::puts(R"(<g class="nodes">)");
	for (std::size_t i = 0; i < fnodes.size(); ++ i) {
		const auto &node  = g.nodes[i];
		const auto &fnode = fnodes[i];
		const NodeDescription &desc = g.node_descriptions.at(node.description);
		float ms = (node.end_ns - node.begin_ns) / 1000'000.0f;
		int title_chars = std::floor(fnode.width * 2 / font_size);
		std::printf(R"(<rect x="%f" y="%f" width="%f" height="%f">)", fnode.x, fnode.y, fnode.width, node_height);
		std::printf("<title>%s:%lu: %s\n%fms", desc.file.c_str(), desc.line, desc.func.c_str(), ms);
		if (size_t(title_chars) < node.label.size()) {
			std::printf("\n%s", node.label.c_str());
		}
		std::puts("</title></rect>");
		std::printf(R"(<text x="%f" y="%f">)", fnode.x + fnode.width / 2, fnode.y + flow_node_text_vpadding);
		if (size_t(title_chars) >= node.label.size()) {
			std::puts(node.label.c_str());
		} else {
			std::printf("%.*s...", std::max(0, title_chars - 3), node.label.c_str());
		}
		std::puts("</text>");
	}
	std::puts("</g>");
	// edges
	std::puts("<defs>");
	for (const auto &edge : g.edges) {
		const auto hit = std::find_if(std::begin(fnodes), std::end(fnodes), [&](const auto &fn) { return fn.task == edge.head; });
		const auto tit = std::find_if(std::begin(fnodes), std::end(fnodes), [&](const auto &fn) { return fn.task == edge.tail; });
		if (hit == std::end(fnodes) || tit == std::end(fnodes)) {
			throw std::runtime_error("Edge references non-existent node");
		}
		const FlowNode &fhead = *hit;
		const FlowNode &ftail = *tit;
		float startx = fhead.x + fhead.width;
		float starty = fhead.y + node_height / 2;
		float endx = ftail.x;
		float endy = ftail.y + node_height / 2;
		float third = (endx - startx) / 3;
		if (starty == endy && (endx - startx) < 10)
			continue;
		std::printf(R"(<path id="E%lu-%lu" d="m%f,%f c%f,%f %f,%f %f,%f">)",
		            edge.head, edge.tail,
		            startx, starty,
		            startx + std::max(third, 15.0f) - startx, 0.0f,
		            endx   - std::max(third, 15.0f) - startx, endy - starty,
		            endx - startx, endy - starty);
		std::printf(R"(<title>%s -> %s</title>)", fhead.node.label.c_str(), ftail.node.label.c_str());
		std::puts("</path>");
	}
	std::puts("</defs><g class=\"EG\"><g>");
	unsigned long prior_head = UINT_MAX;
	for (const auto &edge : g.edges) {
		if (edge.head != prior_head) {
			std::puts("</g><g>");
			prior_head = edge.head;
		}
		std::printf(R"(<use href="#E%lu-%lu"/>)", edge.head, edge.tail);
		std::printf(R"(<use href="#E%lu-%lu"/>)", edge.head, edge.tail);
	}
	std::puts("</g></g></svg>");
}

static std::vector<FlowNode>
flow_nodes(const Graph &g)
{
	std::vector<FlowNode> fnodes;
	for (const auto &node : g.nodes) {
		float x = (node.begin_ns - g.begin_ns) * timescale;
		float y = header_height + thread_lane_vpadding + node.thread * thread_lane_height;
		float width = std::max(1.0f, (node.end_ns - node.begin_ns) * timescale);
		fnodes.emplace_back(FlowNode{node, node.task, x, y, width});
	}
	return fnodes;
}
