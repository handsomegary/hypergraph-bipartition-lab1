"""Run a local input against Lab1 and optionally supplied comparison tools.

Three benchmark inputs are included; third-party executables are optional.
Each command runs in a temporary directory so that generated files stay there.
"""
import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import time

from check_partition import evaluate


def timed(command, directory, limit):
    start = time.perf_counter()
    result = subprocess.run(command, cwd=directory, capture_output=True,
                            text=True, timeout=limit)
    seconds = time.perf_counter() - start
    if result.returncode:
        raise RuntimeError(f'Command failed ({result.returncode}): {result.stderr.strip()}')
    return seconds, result.stdout.strip(), result.stderr.strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('inputs', type=Path, nargs='+')
    parser.add_argument('--solver', type=Path, default=Path('Lab1.exe' if __import__('os').name == 'nt' else 'Lab1'))
    parser.add_argument('--baseline', type=Path,
                        help='Optional external shmetis binary (Linux, sometimes requiring 32-bit support)')
    parser.add_argument('--official-verifier', type=Path,
                        help='Optional external verifier binary (Linux)')
    parser.add_argument('--limit', type=float, default=30)
    args = parser.parse_args()
    solver = args.solver.resolve()
    baseline = args.baseline.resolve() if args.baseline else None
    verifier = args.official_verifier.resolve() if args.official_verifier else None
    reports = []
    for source in args.inputs:
        with tempfile.TemporaryDirectory(prefix='partition-') as temp:
            directory = Path(temp)
            input_path = directory / 'input.hgr'
            shutil.copyfile(source, input_path)
            seconds, _, diagnostic = timed([str(solver), str(input_path)], directory, args.limit)
            output_path = directory / 'output.txt'
            own = evaluate(input_path, output_path)
            if not own['balanced']:
                raise RuntimeError('Solver output is unbalanced')
            own.update(seconds=seconds, diagnostic=diagnostic)
            if verifier:
                _, output, _ = timed([str(verifier), str(input_path), str(output_path)], directory, args.limit)
                own['official_verifier_output'] = output
            report = {'input': source.name, 'solution': own}
            if baseline:
                elapsed, output, _ = timed([str(baseline), str(input_path), '2', '5'], directory, args.limit)
                reference_path = Path(str(input_path) + '.part.2')
                reference = evaluate(input_path, reference_path)
                reference.update(seconds=elapsed, tool_output=output)
                report['baseline'] = reference
            reports.append(report)
    print(json.dumps(reports, indent=2))


if __name__ == '__main__':
    main()
