"""Independently count cut nets and check a two-way partition file."""
import argparse
import json
from pathlib import Path


def read_hypergraph(path):
    with Path(path).open(encoding='utf-8-sig') as source:
        header = None
        for line in source:
            stripped = line.strip()
            if not stripped or stripped.startswith('%'):
                continue
            header = stripped.split()
            break
        if header is None or len(header) != 2:
            raise ValueError('Expected an unweighted header: net_count node_count')
        net_count, node_count = map(int, header)
        if net_count < 0 or node_count < 1:
            raise ValueError('Invalid net or node count')
        nets = []
        for line in source:
            if line.lstrip().startswith('%'):
                continue
            if len(nets) == net_count:
                if line.strip():
                    raise ValueError('Unexpected content after the declared nets')
                continue
            net = set(map(int, line.split()))
            if any(node < 1 or node > node_count for node in net):
                raise ValueError('Node identifier outside 1..node_count')
            nets.append(net)
        if len(nets) != net_count:
            raise ValueError('Fewer net lines than declared in the header')
    return node_count, nets


def evaluate(input_path, output_path, strict=False):
    node_count, nets = read_hypergraph(input_path)
    lines = Path(output_path).read_text(encoding='ascii').splitlines()
    if len(lines) != node_count or any(line.strip() not in ('0', '1') for line in lines):
        raise ValueError('Output must have exactly one 0 or 1 line per node')
    parts = [int(line) for line in lines]
    size_zero = parts.count(0)
    # The supplied checker truncates 0.45*N and uses N-lower as its upper bound.
    lower = (45 * node_count + (99 if strict else 0)) // 100
    upper = node_count - lower
    cut = sum(len({parts[node - 1] for node in net}) > 1 for net in nets)
    return {'nodes': node_count, 'nets': len(nets), 'cut_nets': cut,
            'group0': size_zero, 'group1': node_count - size_zero,
            'group0_fraction': size_zero / node_count,
            'minimum_group_size': lower, 'maximum_group_size': upper,
            'balance_mode': 'strict' if strict else 'supplied-checker',
            'balanced': lower <= size_zero <= upper}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--strict', action='store_true',
                        help='Apply the literal 45%%-55%% interval instead of checker rounding')
    args = parser.parse_args()
    try:
        result = evaluate(args.input, args.output, args.strict)
    except (OSError, ValueError) as error:
        parser.error(str(error))
    print(json.dumps(result, indent=2))
    return 0 if result['balanced'] else 1


if __name__ == '__main__':
    raise SystemExit(main())
