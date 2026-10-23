# Validation

The C++17 solver was compiled locally with optimization and warning flags. An independent Python checker validated the output structure, group sizes, and cut count. The results below describe the available local inputs and checks. They do not establish a score on unavailable hidden inputs or a globally optimal cut on the larger benchmarks.

## Supplied inputs

All three supplied inputs are included in `benchmarks/`. The following local run used the default balance rule that matches the supplied checker. Elapsed times include process startup and are approximate single-run measurements.

| Input | Vertices | Nets | Cut nets | Group 0 | Group 1 | Elapsed seconds |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `input.hgr` | 5 | 3 | 1 | 2 | 3 | 0.638 |
| `input2.txt.hgr` | 6,291 | 6,511 | 91 | 3,210 | 3,081 | 0.069 |
| `input3.txt.hgr` | 85,013 | 89,187 | 20 | 40,641 | 44,372 | 0.614 |

Each output contains exactly one binary label per vertex and satisfies the inclusive bounds `floor(0.45*n)` through `n-floor(0.45*n)`. The small input's two-versus-three split is legal under this integer rule even though its actual proportions are 40% and 60%.

## Additional local checks

The public `tests/regression.py` suite passed 82 local checks:

| Category | Checks | Purpose |
| --- | ---: | --- |
| Supplied inputs | 3 | Validate all included benchmarks |
| Small exact-oracle cases | 48 | Compare the solver's cut with an independent exhaustive search |
| Repeated runs | 10 | Check identical output using two runs on each of five graphs |
| Large structural cases | 3 | Exercise isolated graphs, a giant net, and mixed giant and pair nets |
| Strict balance | 2 | Check feasible and infeasible literal percentage bounds |
| Comments and duplicate pins | 1 | Exercise parser normalization |
| Invalid solver inputs | 10 | Require rejection of malformed netlists |
| Invalid partition outputs | 5 | Check independent-checker rejection cases |

The stress cases include 100,000 isolated vertices, a net spanning 100,000 vertices, and an 80,000-vertex graph with a giant net and pair nets. All produced legal partitions. Strict mode accepted the feasible case and rejected a vertex count for which the literal percentage bounds have no legal integer split.

Exact-oracle agreement applies to the tested small instances. It is separate from the heuristic results on larger inputs. Repeated-run agreement also applies to the tested runs. A different machine or an input that reaches the time budget can stop the search at a different point.

The suite also verified SHA-256 fingerprints for the three original input files and the unchanged source PDF against `benchmarks/source-manifest.json`. Generated inputs and partition outputs use temporary directories.

To reproduce the suite after building, run:

```sh
python3 tests/regression.py --solver ./Lab1
```

The continuous-integration workflow builds and runs that command on Linux. A local passing result does not by itself establish a successful remote CI run.

## Agreement with the supplied verifier logic

The supplied verifier is a Linux executable. Its core checking function was inspected and executed under Unicorn CPU emulation with the required in-memory graph data. C++ stream calls were stubbed. This approach exercises the original function's balance and cut logic, while the repository's Python checker provides an independent implementation.

On all three supplied benchmark outputs, the emulated function and the independent checker agreed on legality, both group sizes, and cut counts of 1, 91, and 20. Additional small cases exercised integer balance boundaries and label handling.

This did not execute the complete Linux verifier process. It does not cover that executable's file parser, dynamic loader, runtime library setup, or command-line execution. The repository's independent checker additionally enforces exactly one `0` or `1` line per vertex.

## Performance and comparison limits

The solver checks a 25-second internal search budget during its work. The lab's external limit is 30 seconds. The internal budget is cooperative rather than a hard wall-clock guarantee, so very large input parsing or individual operations can still add overhead.

The supplied benchmark runs finished well within the external limit on the local machine. The reference `shmetis` binary was not run, and no measured comparison against hMETIS is available. The optional comparison helper is provided for a compatible local Linux environment.

## Documentation

The TeX notes compiled successfully into three pages with two LaTeX passes. All pages were rendered and visually inspected. The compilation produced no overfull or underfull boxes. The repository contains the editable TeX source, while the PDF and compilation intermediates remain local build products.
