"""Exact reference values for the power-of-five table shared by number parsing
(Eisel-Lemire) and printing (Dragonbox).

F(q) is 5^q scaled into [2^127, 2^128) and rounded down, for q in [Q_MIN, Q_MAX].
Both algorithms' tables derive from it:
  Eisel-Lemire (fast_float's table, q in [-342, 308]): F(q) + (1 if -27 <= q < 0 else 0)
  Dragonbox (binary64 cache, k in [-292, 326]):       F(k) + (0 if 0 <= k <= 55 else 1)
(checked entry by entry against both libraries' published tables; see
docs/design-notes.md).
"""

Q_MIN, Q_MAX = -342, 326


def pow5_floor(q):
    """(F(q), whether the scaling is exact)"""
    if q >= 0:
        p = 5 ** q
        s = p.bit_length() - 128
        if s >= 0:
            return p >> s, (p & ((1 << s) - 1)) == 0
        return p << -s, True
    p = 5 ** -q
    b = 127 + p.bit_length()
    while (1 << b) // p >= 1 << 128:
        b -= 1
    while (1 << b) // p < 1 << 127:
        b += 1
    return (1 << b) // p, (1 << b) % p == 0


def table():
    return [pow5_floor(q)[0] for q in range(Q_MIN, Q_MAX + 1)]
