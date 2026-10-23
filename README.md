# Balanced Hypergraph Bipartitioning

An independently written C++17 solution to a two-way hypergraph partitioning lab. The program assigns every vertex to group `0` or `1`, minimizes the number of nets that cross the groups, and respects the allowed group sizes.

The solver uses the standard C++ library. It builds on an ordinary machine with a C++17 compiler and does not require a particular server, a reference executable, or an account on a submission system. Python 3 is optional for the independent output checker and comparison helper.

## Build and run

On Linux or another environment with GNU Make and `g++`:

```sh
make
./Lab1 benchmarks/input.hgr
python3 tools/check_partition.py benchmarks/input.hgr output.txt
```

The executable reads the input path from its argument and writes `output.txt` in the current working directory. It writes a short cut and group-size summary to standard error. A later run in the same directory replaces `output.txt`.

On Windows with a MinGW C++17 compiler, the equivalent commands are:

```powershell
g++ -O3 -std=c++17 -Wall -Wextra -pedantic src/main.cpp -o Lab1.exe
.\Lab1.exe benchmarks/input.hgr
python tools/check_partition.py benchmarks/input.hgr output.txt
```

Run and check all three included inputs with:

```sh
python3 tools/compare.py benchmarks/input.hgr benchmarks/input2.txt.hgr benchmarks/input3.txt.hgr --solver ./Lab1
```

On Windows, use `python` and `--solver .\Lab1.exe`. The helper runs each input in a temporary directory and prints JSON with the cut count, balance result, and elapsed time.

## Input and output

The first input line contains `m n`, the number of nets and vertices. Each of the next `m` lines lists the vertices belonging to one net. Vertex identifiers range from `1` to `n`. The supported input is an unweighted hypergraph. The parser also accepts `%` comment lines and removes repeated identifiers within a net.

The output contains exactly `n` lines. Line `i` is the group label for vertex `i`. A net contributes one cut when it touches both groups, regardless of its number of pins.

The default balance bounds match the supplied checker:

```text
lower = floor(0.45 * n)
upper = n - lower
lower <= size_of_each_group <= upper
```

This integer rounding can allow an actual proportion outside the nominal 45% to 55% interval for very small graphs. To request the literal percentage interval instead, use both strict options:

```sh
./Lab1 benchmarks/input2.txt.hgr --strict-balance
python3 tools/check_partition.py benchmarks/input2.txt.hgr output.txt --strict
```

Strict mode uses `ceil(0.45*n)` as the lower bound. Some small vertex counts have no legal integer split under this interpretation, and the solver reports an error for them.

## Solver approach

For up to 22 vertices, the solver enumerates partitions with incremental cut updates. For larger inputs, it contracts vertices to form a sequence of smaller hypergraphs, constructs several deterministic starting partitions, and applies FM-style refinement while expanding back to the original graph. Contracted vertices retain their original vertex counts as weights so that balance refers to the original graph.

Refinement moves feasible unlocked vertices, updates gains through their incident nets, and keeps the best cumulative-gain prefix of each pass. The solver uses a 25-second internal search budget to leave time for output within the lab's 30-second limit. This is a cooperative budget checked during search, not a hard process timeout. Input parsing, individual operations, and output also take time.

The large-instance method is a heuristic. It does not certify a global minimum. See [validation results and limits](VALIDATION.md).

## Regression checks

After building, run the source-based regression suite with:

```sh
python3 tests/regression.py --solver ./Lab1
```

On Windows, use `python` and `--solver .\Lab1.exe`. The suite checks the included inputs, small graphs against an exhaustive oracle, deterministic repeated runs, large structural cases, strict balance, malformed inputs, and checker rejection cases. It generates additional inputs in temporary directories. The Linux continuous-integration workflow runs the same build and regression command.

## Optional comparison tools

If the original Linux tools are available locally, the comparison helper can invoke them:

```sh
python3 tools/compare.py benchmarks/input2.txt.hgr --solver ./Lab1 --baseline /path/to/shmetis --official-verifier /path/to/verifier
```

The helper calls `shmetis INPUT 2 5`, checks its generated `INPUT.part.2` file, and can also send the solver's output to the supplied verifier. These external executables are not included. A compatible Linux environment may be needed, including 32-bit runtime support for an older reference executable. They are optional and are not needed to build or run this solution.

The independent checker agrees with emulation of the original verifier's core function on the supplied benchmark outputs. That emulation does not execute the complete Linux verifier process. No hMETIS run or measured quality comparison with hMETIS is claimed here.

## Files

| Path | Contents |
| --- | --- |
| `src/main.cpp` | Independent C++17 solver |
| `Makefile` | Builds the required `Lab1` executable |
| `tools/check_partition.py` | Independent output-format, balance, and cut checker |
| `tools/compare.py` | Temporary-directory runner with optional local reference tools |
| `tests/regression.py` | Reproducible regression suite with generated cases and exact oracles |
| `benchmarks/input.hgr` | Supplied small input |
| `benchmarks/input2.txt.hgr` | Supplied medium input |
| `benchmarks/input3.txt.hgr` | Supplied large input |
| `benchmarks/source-manifest.json` | SHA-256 fingerprints for the three original inputs and PDF |
| `docs/lab1_partition_2023.pdf` | Original lab statement, preserved unchanged |
| `docs/problem.md` | Neutral prose restatement and balance-rounding explanation |
| `docs/partition-notes/main.tex` | Original prose summary of the introductory slides |
| `VALIDATION.md` | Local results, verification method, and limitations |

The original slide deck, third-party executables, generated partition outputs, and build products are excluded.

## Compile the TeX notes

With a standard LaTeX installation that includes `amsmath`, `amssymb`, `geometry`, `lmodern`, and `hyperref`:

```sh
mkdir -p .build/notes
pdflatex -interaction=nonstopmode -halt-on-error -output-directory=.build/notes docs/partition-notes/main.tex
pdflatex -interaction=nonstopmode -halt-on-error -output-directory=.build/notes docs/partition-notes/main.tex
```

The resulting `.build/notes/main.pdf` is a readable summary. The TeX source remains editable and has no dependency on the original slide deck.
