Deadlock-Graph
==============

A utility to convert [deadlock](https://github.com/raintherrien/deadlock) graph files to either Graphviz DOT format or a half baked SVG representation.

Usage
-----
Once you've compiled your deadlock library and application to output a graph file `graph.dlg` (this is poorly documented, but you can reference [deadlock/graph.h](https://github.com/raintherrien/deadlock/blob/main/include/deadlock/graph.h)) you may use this application to generate either a [Graphviz DOT](https://graphviz.org/) or custom "Flow" time series graph.

To generate and compile a DOT file:

    deadlock-graph -d graph.dlg > graph.dot
    dot -Tsvg graph.dot -o graph.svg
To generate a Flow SVG:

    deadlock-graph graph.dlg > graph.svg
