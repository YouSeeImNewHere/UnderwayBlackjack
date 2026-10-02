# Infinite-deck basic strategy solver for this game's three rule sets.
# Ranks: 1=A, 2..9, 10 = any ten-value.
from functools import lru_cache
import sys

class Rules:
    def __init__(self, name, probs, spanish=False, freebet=False):
        self.name, self.p, self.spanish, self.freebet = name, probs, spanish, freebet

STD_P = {r: 1/13 for r in range(1,10)}; STD_P[10] = 4/13
SPA_P = {r: 4/48 for r in range(1,10)}; SPA_P[10] = 12/48   # 48-card Spanish deck: no 10s, J/Q/K remain

def add(total, soft, c):
    t = total + c
    s = soft
    if c == 1 and t + 10 <= 21:
        t += 10; s = True
    if t > 21 and s:
        t -= 10; s = False
    return t, s

def dealer_dist(R, up):
    """Dealer final totals (17..21, 22, 'bust'>=23) given up-card, H17, conditioned on no dealer BJ (peek)."""
    p = R.p
    @lru_cache(None)
    def play(total, soft):
        if total > 21:
            return {22 if total == 22 else 99: 1.0}
        if total > 17 or (total == 17 and not soft):
            return {total: 1.0}
        out = {}
        for c, pc in p.items():
            t, s = add(total, soft, c)
            for k, v in play(t, s).items():
                out[k] = out.get(k, 0) + pc * v
        return out
    t0, s0 = add(0, False, up)
    # hole card, conditioned on no blackjack
    hole = dict(p)
    if up == 1: hole.pop(10)
    if up == 10: hole.pop(1)
    z = sum(hole.values())
    out = {}
    for c, pc in hole.items():
        t, s = add(t0, s0, c)
        for k, v in play(t, s).items():
            out[k] = out.get(k, 0) + pc / z * v
    return out

def solve(R):
    res = {}
    for up in range(1, 11):
        D = dealer_dist(R, up)
        def stand_wl(t):
            W = L = 0.0
            for d, pd in D.items():
                if d == 99 or (d == 22 and not R.freebet): W += pd
                elif d == 22: pass                      # Free Bet: dealer 22 pushes
                elif t > d: W += pd
                elif t < d: L += pd
            return W, L

        @lru_cache(None)
        def best(t, soft, n, r, f, splitAces, canSurr, first, dcount=0):
            """Returns (EV, action). r = real stake, f = free stake."""
            # Spanish: a player 21 always wins, with 5/6/7+ card bonuses; reaching 21 auto-stands.
            opts = {}
            if R.spanish and t == 21:
                # 5/6/7+ card bonuses -- not paid on a doubled hand
                m = 1.5 if n == 5 else 2 if n == 6 else 3 if n >= 7 else 1
                if dcount > 0: m = 1
                return (r * m, 'S')
            W, L = stand_wl(t)
            opts['S'] = r * (W - L) + f * W
            if t == 21:
                return (opts['S'], 'S')
            if splitAces and not R.spanish and n >= 2:
                return (opts['S'], 'S')
            if R.spanish and dcount > 0 and dcount >= 3:
                return (opts['S'], 'S')
            # hit (not allowed on a doubled Spanish hand -- only redouble)
            ev = 0.0
            for c, pc in R.p.items():
                nt, ns = add(t, soft, c)
                if nt > 21: ev += pc * (-r)
                else: ev += pc * best(nt, ns, n + 1, r, f, splitAces, False, False)[0]
            if not (R.spanish and dcount > 0):
                opts['H'] = ev
            # double
            canD = (n == 2) or R.spanish
            if splitAces and not R.spanish: canD = False
            if canD:
                amt = r + f
                hard = not soft
                if R.freebet and n == 2 and hard and 9 <= t <= 11:
                    r2, f2 = r, f + amt          # free double
                else:
                    r2, f2 = r + amt, f
                ev = 0.0
                for c, pc in R.p.items():
                    nt, ns = add(t, soft, c)
                    if nt > 21: ev += pc * (-r2)
                    elif R.spanish: ev += pc * best(nt, ns, n + 1, r2, f2, splitAces, False, False, dcount + 1)[0]
                    else:
                        if R.spanish and nt == 21: ev += pc * r2
                        else:
                            W2, L2 = stand_wl(nt)
                            ev += pc * (r2 * (W2 - L2) + f2 * W2)
                opts['D'] = ev
            if canSurr and not R.freebet:
                opts['R'] = -0.5 * r
            a = max(opts, key=lambda k: opts[k])
            return (opts[a], a)

        def split_ev(v):
            aces = v == 1
            def hand(r, f):
                ev = 0.0
                for c, pc in R.p.items():
                    t, s = add(0, False, v); t, s = add(t, s, c)
                    ev += pc * best(t, s, 2, r, f, aces, False, False)[0]
                return ev
            if R.freebet and v != 10:
                return hand(1, 0) + hand(0, 1)     # second hand rides on a free bet
            return 2 * hand(1, 0)

        rows = {}
        for t in range(5, 18):                    # hard totals (two-card, non-pair representative)
            rows[('H', t)] = best(t, False, 2, 1, 0, False, True, True)
        for x in range(2, 10):                    # soft A+x
            t, s = add(1, False, 1); t, s = add(0, False, 1); t, s = add(t, s, x)
            rows[('S', x)] = best(t, s, 2, 1, 0, False, True, True)
        for v in range(1, 11):                    # pairs
            t, s = add(0, False, v); t, s = add(t, s, v)
            ev, a = best(t, s, 2, 1, 0, False, True, True)
            sp = split_ev(v)
            rows[('P', v)] = ('P', sp) if sp > ev else (a, ev)
        res[up] = rows
    return res

def chart(R):
    res = solve(R)
    cols = [2,3,4,5,6,7,8,9,10,1]
    hard = [''.join(res[u][('H', t)][1] for u in cols) for t in range(8, 18)]
    hard[0] = ''.join(res[u][('H', 8)][1] for u in cols)
    soft = [''.join(res[u][('S', x)][1] for u in cols) for x in range(2, 10)]
    pair = [''.join(res[u][('P', v)][0] for u in cols) for v in [2,3,4,5,6,7,8,9,10,1]]
    return hard, soft, pair

if __name__ == '__main__':
    for R in [Rules('STANDARD', STD_P), Rules('SPANISH21', SPA_P, spanish=True), Rules('FREEBET', STD_P, freebet=True)]:
        h, s, p = chart(R)
        print(R.name)
        print(' HARD 8-17 ', h)
        print(' SOFT A2-A9', s)
        print(' PAIR 2..A ', p)
