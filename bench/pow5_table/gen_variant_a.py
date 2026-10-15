"""Generates the constants of variant A (compressed table) and checks that the
recovery the C++ code performs reproduces every F(q) exactly."""

from pow5_reference import Q_MIN, Q_MAX, pow5_floor

RATIO = 27
GRID0 = -351  # bases on multiples of 27, so q = 0, 27, 54 are exact bases


def recover(base, off):
    prod = base * 5 ** off
    return prod >> (prod.bit_length() - 128)


bases = [pow5_floor(qb)[0] for qb in range(GRID0, Q_MAX + 1, RATIO)]
corrections = []
for q in range(Q_MIN, Q_MAX + 1):
    i, off = divmod(q - GRID0, RATIO)
    delta = pow5_floor(q)[0] - recover(bases[i], off)
    assert 0 <= delta <= 3, (q, delta)
    corrections.append(delta)

words = []
for start in range(0, len(corrections), 32):
    w = 0
    for j, d in enumerate(corrections[start:start + 32]):
        w |= d << (2 * j)
    words.append(w)

print(f'// {len(bases)} bases (F at q = {GRID0} + {RATIO}i), {len(words)} correction words')
print('inline constexpr u128 bases[] = {')
for b in bases:
    print(f'    {{0x{b >> 64:016x}, 0x{b & (2**64 - 1):016x}}},')
print('};')
print('inline constexpr std::uint64_t corrections[] = {')
for w in words:
    print(f'    0x{w:016x},')
print('};')
