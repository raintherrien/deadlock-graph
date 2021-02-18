#include "graph.h"

#include <chrono>
#include <climits>
#include <cmath>
#include <cstdio>

struct FlowNode {
	const Node &node;
	unsigned long task;
	int x, y, width;
};

struct FlowEdgeSpawnAnnotation {
	int ts_ns, x, y;
	bool top;
};

struct FlowEdgeArrow {
	int x, y;
};

enum FlowEdgeFlags : unsigned {
	None         = 0,
	Intermediate = 1 << 0 // edge begins within node, with timestamp
};

struct FlowEdge {
	unsigned long head;
	unsigned long tail;
	int startx;
	int starty;
	int endx;
	int endy;
	enum FlowEdgeFlags flags;
};

static std::vector<FlowNode> flow_nodes(const Graph &g, double timescale);

constexpr int header_height = 32;
constexpr int font_size = 16;
constexpr int node_height = 48;
constexpr int thread_lane_height = 96;
constexpr int thread_lane_vpadding = (thread_lane_height - node_height) / 2;
constexpr int node_text_hpadding = 8;
constexpr int node_text_vpadding = node_height / 2;
constexpr int sharpest_edge_offset = 32;
constexpr int edge_triangle_offset = 5;
constexpr int edge_annotation_vpadding = 2;
constexpr int edge_spawn_annotation_buffer = 8;

void
dump_flow(const Graph &g, double timescale)
{
	const auto fnodes = flow_nodes(g, timescale);

	unsigned long long time_delta = g.end_ns - g.begin_ns;
	if (time_delta * timescale > std::numeric_limits<int>::max()) {
		throw std::runtime_error("Timescale out of range");
	}
	int width  = time_delta * timescale;
	int height = header_height + g.num_threads * thread_lane_height;

	std::puts(R"(<?xml version="1.0" encoding="UTF-8" standalone="no"?>)");
	std::printf(R"(<svg width="%d" height="%d" viewBox="0 0 %d %d" xmlns="http://www.w3.org/2000/svg">)",
	            width, height, width, height);
	// styles
	std::printf("<style>"
	            "text{font-size:%dpx;stroke:none;dominant-baseline:middle}"
	            "path{fill:none}"
	            ".nodes rect{fill:lightgray;stroke:black}"
	            ".nodes text{fill:black}"
	            "path.C{stroke:black;fill:lavender;stroke-dasharray:4;stroke-width:0.5}"
	            "rect.C{fill:lavender}"
	            ".EG g use:nth-child(1n){stroke-width:5;pointer-events:stroke}"
	            ".EG g use:nth-child(2n){stroke-width:1;stroke:black}"
	            ".EG g:hover use:nth-child(2n){stroke:red;stroke-opacity:1;stroke-width:2}"
	            ".Ea{font-size:%dpx}"
	            "</style>", font_size, font_size / 3);
	// thread dividers
	std::puts(R"(<g stroke="gray" stroke-width="0.3" stroke-dasharray="2">)");
	for (unsigned long i = 0; i < g.num_threads+1; ++ i) {
		int y = header_height + i * thread_lane_height;
		std::printf(R"(<line x2="%d" y1="%d" y2="%d"/>)", width, y, y);
	}
	std::puts("</g>");
	// timestamps
	std::puts(R"(<g stroke="crimson" stroke-width="0.3">)");
	double timestamp_interval;
	const char *timestamp_header;
	if (time_delta > 1e9) {
		timestamp_interval = 1e9 / 60.0; // every 60Hz (16.666ms)
		timestamp_header = "16.666ms";
	} else if (time_delta > 100e6) {
		timestamp_interval = 1e9 / 120.0; // every 120Hz (8.333ms)
		timestamp_header = "8.333ms";
	} else if (time_delta > 1e6) {
		timestamp_interval = 1e6; // every 1ms
		timestamp_header = "1ms";
	} else if (time_delta > 10e3) {
		timestamp_interval = 100e3; // every 100us
		timestamp_header = "100μs";
	} else {
		timestamp_interval = 50e3; // every 50us
		timestamp_header = "50μs";
	}
	double maxt = (g.end_ns - g.begin_ns) / timestamp_interval;
	for (double t = 1; t < maxt; ++ t) {
		int x = std::lroundf(t * timestamp_interval * timescale);
		std::printf(R"(<line x1="%d" x2="%d" y1="%d" y2="%d"/>)", x, x, header_height, height);
	}
	std::printf(R"(<text x="%f" y="%f">%s</text>)", timestamp_interval * timescale, header_height * .75, timestamp_header);
	std::puts("</g>");
	// continuations
	for (const auto &continuation : g.continuations) {
		const auto hit = std::find_if(std::begin(fnodes), std::end(fnodes), [&](const auto &fn) { return fn.task == continuation.head; });
		const auto tit = std::find_if(std::begin(fnodes), std::end(fnodes), [&](const auto &fn) { return fn.task == continuation.tail; });
		if (hit == std::end(fnodes) || tit == std::end(fnodes)) {
			throw std::runtime_error("Continuation references non-existent node");
		}
		const FlowNode &fhead = *hit;
		const FlowNode &ftail = *tit;
		int sx = fhead.x + fhead.width;
		int sy = fhead.y;
		int ex = ftail.x;
		int ey = ftail.y;
		int mx = (sx + ex) / 2;
		int my = (sy + ey) / 2;
		std::printf(R"(<path class="C" d="m%d,%d L%d,%d Q%d,%d  %d,%d   %d,%d   %d,%d L%d,%d Q%d,%d %d,%d %d,%d %d,%d"/>)",
		            sx, sy + node_height, // start at node right edge bottom corner
		            sx, sy,               // line to node right edge top corner

		            // quadratic curve from right edge top corner to left edge top corner
		            mx, sy,
		            mx, my,
		            mx, ey,
		            ex, ey,

		            ex, ey + node_height,  // line to node left edge bottom corner

		            // quadratic curve from left edge bottom corner to right edge bottom corner
		            mx, ey + node_height,
		            mx, my + node_height,
		            mx, sy + node_height,
		            sx, sy + node_height);
	}
	// nodes
	std::puts(R"(<g class="nodes">)");
	for (std::size_t i = 0; i < fnodes.size(); ++ i) {
		const auto &node  = g.nodes[i];
		const auto &fnode = fnodes[i];
		const NodeDescription &desc = g.node_descriptions.at(node.description);
		float ms = (node.end_ns - node.begin_ns) / 1000'000.0f;
		int title_chars = std::floor(fnode.width * 2 / font_size);
		bool is_continuation = g.continuations.end() != std::find_if(g.continuations.begin(), g.continuations.end(), [&](const auto &c) { return c.tail == node.task; }); // XXX bad hack
		std::printf(R"(<rect %s x="%d" y="%d" width="%d" height="%d">)", is_continuation ? R"(class="C")" : "", fnode.x, fnode.y, fnode.width, node_height);
		std::printf("<title>%s:%lu: %s\n%fms", desc.file.c_str(), desc.line, desc.func.c_str(), ms);
		if (size_t(title_chars) < node.label.size()) {
			std::printf("\n%s", node.label.c_str());
		}
		std::puts("</title></rect>");
		std::printf(R"(<text x="%d" y="%d">)", fnode.x + node_text_hpadding, fnode.y + node_text_vpadding);
		if (size_t(title_chars) >= desc.func.size() + node.label.size()) {
			std::printf("%s: %s", desc.func.c_str(), node.label.c_str());
		} else if (size_t(title_chars) >= node.label.size()) {
			std::puts(node.label.c_str());
		} else {
			std::printf("%.*s...", std::max(0, title_chars - 3), node.label.c_str());
		}
		std::puts("</text>");
	}
	std::puts("</g>");
	// edges
	std::vector<FlowEdgeSpawnAnnotation> edge_spawn_annotations;
	std::vector<FlowEdgeArrow> edge_arrows;
	std::vector<FlowEdge> edges;
	for (const auto &edge : g.edges) {
		const auto hit = std::find_if(std::begin(fnodes), std::end(fnodes), [&](const auto &fn) { return fn.task == edge.head; });
		const auto tit = std::find_if(std::begin(fnodes), std::end(fnodes), [&](const auto &fn) { return fn.task == edge.tail; });
		if (hit == std::end(fnodes) || tit == std::end(fnodes)) {
			throw std::runtime_error("Edge references non-existent node");
		}
		const FlowNode &fhead = *hit;
		const FlowNode &ftail = *tit;
		int endx = ftail.x;
		int endy = ftail.y + node_height / 2;
		edge_arrows.emplace_back(FlowEdgeArrow{endx, endy});
		if (fhead.y == ftail.y && (double(ftail.node.begin_ns) - fhead.node.end_ns) * timescale < 10) {
			continue; // skip edges shorter than arrow
		} else if ((double(fhead.node.end_ns) - edge.ts_ns) * timescale < 8) {
			// path from end of node
			int startx = fhead.x + fhead.width;
			int starty = fhead.y + node_height / 2;
			edges.emplace_back(FlowEdge{edge.head, edge.tail, startx, starty, endx, endy, FlowEdgeFlags::None});
		} else {
			// path from intra-node
			bool bottom = ftail.y > fhead.y;
			int dns = edge.ts_ns - fhead.node.begin_ns;
			int startx = fhead.x + dns * timescale;
			int starty = fhead.y + (bottom ? node_height : 0);
			edges.emplace_back(FlowEdge{edge.head, edge.tail, startx, starty, endx, endy, FlowEdgeFlags::Intermediate});
			starty += bottom ? -edge_annotation_vpadding : edge_annotation_vpadding;
			edge_spawn_annotations.emplace_back(FlowEdgeSpawnAnnotation{dns, startx, starty, !bottom});
		}
	}
	// Sort annotations and trim overlapping
	std::sort(edge_spawn_annotations.begin(),
	          edge_spawn_annotations.end(),
	          [](const auto &a, const auto &b) {
			if (a.y < b.y) return true;
			if (b.y < a.y) return false;
			if (a.x < b.x) return true;
			if (a.x > b.x) return false;
			return a.ts_ns < b.ts_ns; // bogus
		});
	for (auto it = edge_spawn_annotations.begin(); it != edge_spawn_annotations.end();) {
		auto next = std::next(it);
		if (next == edge_spawn_annotations.end())
			break;
		if (next->y == it->y && std::abs(next->x - it->x) < edge_spawn_annotation_buffer) {
			it = std::prev(edge_spawn_annotations.erase(next));
		} else {
			++ it;
		}
	}
	for (const auto &annotation : edge_spawn_annotations) {
		std::printf(R"#(<text class="Ea" transform="translate(%d,%d) rotate(-60)" style="text-anchor:%s;">%dμs</text>)#",
		            annotation.x, annotation.y,
		            annotation.top ? "end" : "start",
		            annotation.ts_ns / 1000);
	}
	for (const auto &arrow : edge_arrows) {
		std::printf(R"(<polygon points="%d,%d %d,%d %d,%d"/>)",
		            arrow.x - edge_triangle_offset, arrow.y + 5,
		            arrow.x - edge_triangle_offset, arrow.y - 5,
		            arrow.x + edge_triangle_offset, arrow.y);
	}
	std::puts("<defs>");
	for (const auto &edge : edges) {
		if (edge.flags & FlowEdgeFlags::Intermediate) {
			std::printf(R"(<path id="E%lu-%lu" d="m%d,%d Q%d,%d %d,%d"/>)",
			            edge.head, edge.tail,
			            edge.startx, edge.starty,
			            std::min(edge.endx - sharpest_edge_offset, edge.startx), edge.endy,
			            edge.endx - edge_triangle_offset, edge.endy);
		} else {
			int midx = (edge.startx + edge.endx) / 2;
			int midy = (edge.starty + edge.endy) / 2;
			std::printf(R"(<path id="E%lu-%lu" d="m%d,%d Q%d,%d %d,%d %d,%d %d,%d"/>)",
			            edge.head, edge.tail,
			            edge.startx, edge.starty,
			            std::max(edge.startx + sharpest_edge_offset, midx), edge.starty,
			            midx, midy,
			            std::min(edge.endx - sharpest_edge_offset, midx), edge.endy,
			            edge.endx - edge_triangle_offset, edge.endy);
		}

	}

	std::puts("</defs><g class=\"EG\"><g>");
	unsigned long prior_head = std::numeric_limits<unsigned long>::max();
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
flow_nodes(const Graph &g, double timescale)
{
	std::vector<FlowNode> fnodes;
	for (const auto &node : g.nodes) {
		int x = (node.begin_ns - g.begin_ns) * timescale;
		int y = header_height + thread_lane_vpadding + node.thread * thread_lane_height;
		int width = std::max(1.0, (node.end_ns - node.begin_ns) * timescale);
		fnodes.emplace_back(FlowNode{node, node.task, x, y, width});
	}
	return fnodes;
}
