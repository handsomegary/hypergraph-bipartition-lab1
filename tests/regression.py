"""Check bundled inputs, independent small-case optima, edge cases and errors.

Requires only Python 3.9+ and a previously built solver. All outputs are temporary.
"""
import argparse
import hashlib
import importlib.util
import itertools
import json
import os
from pathlib import Path
import random
import subprocess
import tempfile
import time

ROOT = Path(__file__).resolve().parent.parent
SPEC = importlib.util.spec_from_file_location('checker', ROOT / 'tools/check_partition.py')
CHECKER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(CHECKER)


def require(condition, message):
    if not condition:
        raise AssertionError(message)


def write_graph(path, nodes, nets):
    path.write_text(f'{len(nets)} {nodes}\n' + ''.join(
        ' '.join(str(v + 1) for v in net) + '\n' for net in nets), encoding='ascii')


def optimum(nodes, nets):
    """Enumerate legal assignments independently, fixing vertex 0 by symmetry."""
    lower = 45 * nodes // 100
    if lower == 0:
        return 0
    masks = [sum(1 << v for v in set(net)) for net in nets]
    best = len(nets)
    for ones in range(lower, nodes - lower + 1):
        for rest in itertools.combinations(range(1, nodes), ones - 1):
            bits = 1 + sum(1 << v for v in rest)
            best = min(best, sum(0 < (mask & bits) < mask for mask in masks))
    return best


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--solver', type=Path,
                        default=ROOT / ('Lab1.exe' if os.name == 'nt' else 'Lab1'))
    parser.add_argument('--json', type=Path, help='Save a machine-readable report')
    args = parser.parse_args()
    solver = args.solver.resolve()
    require(solver.is_file(), f'Build the solver first: {solver}')
    records = []

    # Verify that the assignment PDF and provided data retain their original bytes.
    manifest = json.loads((ROOT / 'benchmarks/source-manifest.json').read_text(encoding='utf-8'))
    for item in manifest['files']:
        data = (ROOT / item['path']).read_bytes()
        require(len(data) == item['bytes'] and hashlib.sha256(data).hexdigest() == item['sha256'],
                f'Original file changed: {item["path"]}')

    with tempfile.TemporaryDirectory(prefix='partition-regression-') as temporary:
        temporary = Path(temporary)

        def run(path, label, expected=None, strict=False, reject=False):
            directory = temporary / label
            directory.mkdir()
            command = [str(solver), str(path.resolve())]
            if strict:
                command.append('--strict-balance')
            start = time.perf_counter()
            result = subprocess.run(command, cwd=directory, text=True,
                                    capture_output=True, timeout=30)
            seconds = time.perf_counter() - start
            output = directory / 'output.txt'
            if reject:
                require(result.returncode != 0 and not output.exists(),
                        f'{label}: invalid input accepted: {result.stderr}')
                records.append({'label': label, 'rejected': True})
                return None
            require(result.returncode == 0, f'{label}: {result.stderr}')
            report = CHECKER.evaluate(path, output, strict)
            require(report['balanced'], f'{label}: unbalanced {report}')
            if expected is not None:
                require(report['cut_nets'] == expected,
                        f'{label}: cut {report["cut_nets"]}, expected {expected}')
            report.update(label=label, seconds=seconds, exact_optimum=expected)
            records.append(report)
            return output.read_bytes()

        for name, expected in [('input.hgr', 1), ('input2.txt.hgr', None),
                               ('input3.txt.hgr', None)]:
            run(ROOT / 'benchmarks' / name, f'provided-{name}', expected)

        rng = random.Random(173)
        cases = [(1, [[0]]), (2, []), (2, [[0, 1]]),
                 (5, [[0, 1, 2], [1, 3], [0, 4]]),
                 (8, [[], [0], [1, 1], [0, 1], [0, 1], [2, 3], [4, 5], [6, 7]]),
                 (7, [[i, i + 1] for i in range(6)]), (9, [list(range(9))]),
                 (10, [[0, 1, 2, 3], [4, 5, 6, 7, 8, 9]])]
        for _ in range(40):
            nodes = rng.randrange(2, 14)
            nets = [rng.sample(range(nodes), rng.randrange(1, nodes + 1))
                    for _ in range(rng.randrange(0, 24))]
            cases.append((nodes, nets))
        for i, (nodes, nets) in enumerate(cases):
            path = temporary / f'exact-{i}.hgr'
            write_graph(path, nodes, nets)
            run(path, f'exact-{i}', optimum(nodes, nets))

        for i, nodes in enumerate([26, 31, 64, 125, 300]):
            nets = [[v, (v + 1) % nodes] for v in range(nodes)]
            nets += [rng.sample(range(nodes), rng.randrange(2, min(nodes, 12)))
                     for _ in range(nodes)]
            path = temporary / f'heuristic-{i}.hgr'
            write_graph(path, nodes, nets)
            first = run(path, f'heuristic-{i}')
            second = run(path, f'heuristic-repeat-{i}')
            require(first == second, f'Unexpected nondeterminism: {i}')

        for label, nodes, nets, cut in [
                ('isolated-large', 100000, [], 0),
                ('giant-large', 100000, [range(100000)], 1),
                ('giant-plus-pairs', 80000,
                 [range(80000)] + [[i, i + 1] for i in range(0, 80000, 2)], 1)]:
            path = temporary / f'{label}.hgr'
            write_graph(path, nodes, nets)
            run(path, label, cut)

        for nodes, reject in [(5, True), (20, False)]:
            path = temporary / f'strict-{nodes}.hgr'
            write_graph(path, nodes, [range(nodes)])
            run(path, f'strict-{nodes}', 1, strict=True, reject=reject)

        valid_comments = temporary / 'comments.hgr'
        valid_comments.write_text('% example\n\n2 4\n% first\n1 2 2\n3 4\n% end\n', encoding='ascii')
        run(valid_comments, 'comments-and-duplicate-pins', 0)

        invalid = ['', '1 0\n1\n', '1 2 1\n1 2\n', '2 3\n1 2\n',
                   '0 2\n1 2\n', '1 2\n0 2\n', '1 2\n1 3\n',
                   '1 2\n1 x\n', '1 2\n1 2 99999999999999999999\n',
                   '1 2\n1 2 -99999999999999999999\n']
        for i, text in enumerate(invalid):
            path = temporary / f'invalid-{i}.hgr'
            path.write_text(text, encoding='ascii')
            run(path, f'invalid-{i}', reject=True)

        checker_input = temporary / 'checker.hgr'
        write_graph(checker_input, 4, [[0, 1], [2, 3]])
        for i, text in enumerate(['0\n1\n', '0\n1\n0\n1\n0\n',
                                   '0\n1\n2\n1\n', '0\n\n0\n1\n']):
            output = temporary / f'bad-output-{i}.txt'
            output.write_text(text, encoding='ascii')
            try:
                CHECKER.evaluate(checker_input, output)
            except ValueError:
                records.append({'label': f'checker-invalid-{i}', 'rejected': True})
            else:
                raise AssertionError(f'Checker accepted malformed output {i}')
        unbalanced = temporary / 'unbalanced.txt'
        unbalanced.write_text('0\n0\n0\n0\n', encoding='ascii')
        require(not CHECKER.evaluate(checker_input, unbalanced)['balanced'],
                'Checker accepted an unbalanced partition')
        records.append({'label': 'checker-unbalanced', 'rejected': True})

    report = {'passed': True, 'checks': len(records), 'source_files_verified': len(manifest['files']),
              'results': records}
    if args.json:
        args.json.write_text(json.dumps(report, indent=2) + '\n', encoding='utf-8')
    for record in records[:3]:
        print(f'{record["label"]}: cut={record["cut_nets"]}, '
              f'groups={record["group0"]}/{record["group1"]}, {record["seconds"]:.3f}s')
    print(f'PASS: {len(records)} checks; {len(manifest["files"])} original-file hashes verified')


if __name__ == '__main__':
    main()
