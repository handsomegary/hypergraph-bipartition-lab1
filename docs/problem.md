# Balanced hypergraph bipartitioning

This is an original, anonymized restatement of the supplied lab specification. The [original assignment PDF](lab1_partition_2023.pdf) is also included. The slide deck is summarized as [editable TeX notes](partition-notes/main.tex).

## Task

Write a program that divides the vertices of an unweighted hypergraph into two groups. A net is a hyperedge that may connect more than two vertices. Minimize the number of nets that contain vertices from both groups while keeping the group sizes balanced. Count each cut net once, regardless of how many of its vertices lie in each group.

Any suitable partitioning method is allowed. The source material discusses Kernighan-Lin and Fiduccia-Mattheyses refinement, and suggests comparing the result with hMETIS. A legal partition is the first requirement. Cut quality has higher priority than execution time, and the original evaluation allows at most 30 seconds per input. An arbitrary random assignment is not an adequate solution.

## Input

The command-line argument names a plain-text netlist file. Its first line contains two integers, `m n`, giving the number of nets and vertices. Vertex identifiers run from `1` through `n`. Each of the following `m` lines lists the vertex identifiers incident to one net. A line boundary separates one net from the next.

The lab uses the unweighted form of the hypergraph format. Optional hMETIS weight-format fields are outside the stated input contract.

## Output

Create `output.txt` in the current working directory. Write exactly `n` lines. Line `i` contains `0` or `1`, identifying the group assigned to vertex `i`. The names of the two groups are interchangeable.

## Balance and integer rounding

The statement describes a nominal 45% to 55% balance range. Inspection of the supplied checker shows that it applies the following integer rule:

```text
lower = floor(0.45 * n)
upper = n - lower
lower <= number_of_vertices_in_group_0 <= upper
```

The same bounds then hold for group 1. This rounding can permit actual ratios outside 45% to 55% for small graphs. For example, with five vertices the accepted group sizes are two and three. A literal percentage constraint would instead require `ceil(0.45*n) <= size <= floor(0.55*n)` and can be impossible for small `n`. The implementation follows the supplied checker's integer rule.

## Build and invocation contract

Running `make` must produce an executable named `Lab1`. Invoke it as:

```sh
./Lab1 path/to/netlist.hgr
```

The PDF specifies the capitalized executable name `Lab1`. The introductory slides use a lowercase spelling in an example. This repository follows the PDF naming rule.

The original checking interface is:

```sh
./verifier path/to/netlist.hgr output.txt
```

The supplied checker and reference partitioner are Linux executables. They are optional tools for local comparison. The solution and its own output checks should be buildable from source without an institution-specific server.

## Local comparison with a reference partitioner

If an hMETIS `shmetis` executable is available locally, the introductory material demonstrates the invocation below. Its arguments select two groups and an imbalance parameter of five:

```sh
./shmetis path/to/netlist.hgr 2 5
```

That tool writes the assignments to the input path with `.part.2` appended. Evaluate any such output with the same balance and cut definitions used for the solution. The parameter's interpretation belongs to the reference tool and should not be assumed to replace the checker's integer rule.

## Repository scope

The repository contains an independently written solution, build instructions, the original assignment PDF, all three supplied benchmark inputs, regression checks, and prose notes. It excludes the reference executables, original slide deck, and generated build products. This restatement omits administrative instructions and does not claim that local validation establishes a result on unavailable hidden inputs.
