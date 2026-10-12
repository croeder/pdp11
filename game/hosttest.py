#!/usr/bin/env python3
"""Checks the PDP-11 build of the game against a build for this computer.

The same rogue.c is compiled with the host's C compiler (with host/console.c
in place of console.c), then both play the same random key sequences: the
PDP-11 build on an emulator, the host build directly.  Their output must be
byte-identical.  A difference means a miscompilation (as with GCC's bit-test
code, see the Makefile), an emulator bug, or code whose result depends on
the size of int.

    python3 hosttest.py [c|go|rust|python ...] [--games N] [--keys N]

Needs the cross compiler (to build rogue.lst), a host C compiler, and the
emulators (run.sh builds them).  Default: rust, 20 games of 300 keys.
"""
import argparse
import os
import random
import re
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))


def build_host(work):
    for f in ('rogue.c', 'host/console.h', 'host/console.c'):
        shutil.copy(os.path.join(HERE, f), work)
    exe = os.path.join(work, 'rogue-host')
    # host/console.c has the real main, so only rogue.c's main is renamed
    subprocess.run(['cc', '-O2', '-w', '-c', 'console.c'], cwd=work, check=True)
    subprocess.run(['cc', '-O2', '-w', '-c', '-Dmain=game_main', 'rogue.c'], cwd=work, check=True)
    subprocess.run(['cc', '-o', exe, 'rogue.o', 'console.o'], cwd=work, check=True)
    return exe


WALKABLE = set('.#+>*!&@')
STEPS = {(-1, 0): 'h', (1, 0): 'l', (0, -1): 'k', (0, 1): 'j',
         (-1, -1): 'y', (1, -1): 'u', (-1, 1): 'b', (1, 1): 'n'}


class Bot:
    """Plays the host build: reads the screen after each key (the output for
    a longer key sequence extends the output for a shorter one, so it feeds
    only the new bytes), and picks the next key.  It fights whatever is
    next to it, heads for the stairs or the Amulet, then items, then doors
    and corridors it hasn't stood on, and now and then does something
    random so games differ."""

    def __init__(self, host, rng):
        import pyte
        self.host, self.rng = host, rng
        self.screen = pyte.Screen(80, 24)
        self.stream = pyte.Stream(self.screen)
        self.fed = 0
        self.visited = set()
        self.stairs = None
        self.level = None

    def look(self, keys):
        out = subprocess.run([self.host, keys], capture_output=True, check=True).stdout
        self.stream.feed(out[self.fed:].decode('latin-1'))
        self.fed = len(out)
        rows = [line for line in self.screen.display]
        return rows, out

    def next_key(self, rows):
        cells = {(x, y): rows[y][x] for y in range(1, 22) for x in range(79)}
        if rows[23][:8] != self.level:          # "Level:N " changed: a new map
            self.level = rows[23][:8]
            self.visited, self.stairs = set(), None
        you = next((p for p, c in cells.items() if c == '@'), None)
        if you is None:
            return 'q'
        self.visited.add(you)
        for p, c in cells.items():
            if c == '>':
                self.stairs = p                 # the screen shows @ when you stand on it
        if self.rng.random() < 0.08:
            return self.rng.choice('hjklyubn.')
        for (dx, dy), key in STEPS.items():
            if cells.get((you[0] + dx, you[1] + dy), ' ').isalpha():
                return key                      # fight
        if you == self.stairs:
            return '>'
        hp = re.search(r'Hp:(\d+)\((\d+)\)', rows[23])
        if hp and int(hp.group(1)) * 2 < int(hp.group(2)):
            return '.'                          # rest while hurt and alone
        stairs = [p for p, c in cells.items() if c in '>&']
        for goals in (stairs,
                      [p for p, c in cells.items() if c in '*!'],
                      [p for p, c in cells.items() if c in '+#' and p not in self.visited],
                      [p for p, c in cells.items() if c == '.' and p not in self.visited]):
            step = self.path(cells, you, set(goals))
            if step:
                return step
        return self.rng.choice('hjklyubn')

    @staticmethod
    def path(cells, start, goals):
        """The first step of a shortest path to a goal over walkable cells
        (monsters count as walkable: stepping into one attacks it)."""
        if not goals:
            return None
        first = {start: None}
        frontier = [start]
        while frontier:
            nxt = []
            for p in frontier:
                for (dx, dy), key in STEPS.items():
                    q = (p[0] + dx, p[1] + dy)
                    c = cells.get(q, ' ')
                    if q in first or not (c in WALKABLE or c.isalpha()):
                        continue
                    first[q] = first[p] or key
                    if q in goals:
                        return first[q]
                    nxt.append(q)
            frontier = nxt
        return None


def bot_game(host, rng, max_keys):
    """A key sequence from the bot: a dungeon digit, then up to max_keys
    keys, then q, which ends the game and stops the emulator (unless the
    player has died or won by then)."""
    bot = Bot(host, rng)
    keys = str(rng.randint(1, 9))
    for _ in range(max_keys):
        rows, out = bot.look(keys)
        if b'You died' in out or b'You won' in out:
            return keys
        keys += bot.next_key(rows)
    return keys + 'q'


def first_difference(a, b):
    for i, (x, y) in enumerate(zip(a, b)):
        if x != y:
            return i
    return min(len(a), len(b))


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('emulators', nargs='*', help='c, go, rust or python (default rust)')
    parser.add_argument('--games', type=int, default=20)
    parser.add_argument('--keys', type=int, default=300)
    parser.add_argument('--seed', type=int, default=1)
    args = parser.parse_args()
    emulators = args.emulators or ['rust']
    for e in emulators:
        if e not in ('c', 'go', 'rust', 'python'):
            parser.error(f'unknown emulator {e!r}: use c, go, rust or python')

    subprocess.run(['make', '-s', '-C', HERE, 'rogue.lst'], check=True)
    rng = random.Random(args.seed)
    failures = 0
    with tempfile.TemporaryDirectory() as work:
        host = build_host(work)
        for game in range(args.games):
            keys = bot_game(host, rng, args.keys)
            expected = subprocess.run([host, keys], capture_output=True, check=True).stdout
            limit = str(5_000_000 + 1_000_000 * len(keys))
            for emulator in emulators:
                got = subprocess.run([os.path.join(HERE, 'run.sh'), emulator, limit],
                                     input=keys.encode(), capture_output=True).stdout
                if got == expected:
                    continue
                failures += 1
                at = first_difference(got, expected)
                print(f'game {game} on {emulator}: output differs at byte {at} '
                      f'({len(got)} bytes, host {len(expected)}); keys {keys!r}')
                print(f'  {emulator}: {got[max(0, at - 40):at + 40]!r}')
                print(f'  host: {expected[max(0, at - 40):at + 40]!r}')
            print(f'game {game}: {len(expected)} bytes of output', file=sys.stderr)
    total = args.games * len(emulators)
    print(f'{total - failures} of {total} games identical to the host build '
          f'({args.games} games of {args.keys} keys on {", ".join(emulators)})')
    sys.exit(1 if failures else 0)


if __name__ == '__main__':
    main()
