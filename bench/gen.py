#!/usr/bin/python3
import argparse
import bisect
import os
import random
import subprocess as sp

from datetime import date
from operator import itemgetter

def run(c):
  out = sp.run(c, capture_output=True)
  return out.stdout.removesuffix(b'\n').decode('ascii')

class Commit:
  def __init__(self, logline):
    (hash, timestamp) = logline.split()
    self.hash = hash
    self.date = date.fromisoformat(timestamp)

  def __repr__(self):
    return str(self)

  def __str__(self):
    return f"{self.hash}@({str(self.date)})"

  def __lt__(self, other):
    try:
      return self.date < other.date
    except AttributeError:
      return self.date < other

def schedule(args):
  cwd = os.getcwd()
  os.chdir(args.linux)
  commits = run(['git', 'log',
                 '--after', f'{args.months+1} months ago',
                 '--format=%H %cs'])
  commits = [Commit(s) for s in commits.split('\n')]
  commits.sort()
  schedule = []
  added = set()
  begi = len(commits)
  for _ in range(args.months):
    endi = begi - 1
    if endi < 0:
      break
    begd = commits[endi].date.replace(day=1)
    begi = bisect.bisect_left(commits, begd)
    print(f"{endi-begi+1} commits for {str(begd)}")
    for _ in range(args.days_per_month):
      c = commits[random.randrange(begi, endi+1)]
      if c.hash not in added:
        added.add(c.hash)
        schedule.append(c)
  with open(cwd + '/' + args.out, 'w') as f:
    for c in schedule:
      print(c.hash, file=f)

def mktars(args):
  out = os.getcwd() + '/' + args.out
  try:
    os.mkdir(out)
  except FileExistsError:
    pass
  commits = open(args.schedule, 'r').read().split()
  os.chdir(args.linux)
  for c in commits:
    sp.run(['git', 'checkout', c]).check_returncode()
    tarf = out + '/' + c + '.tar'
    sp.run(['tar', '--exclude-vcs', '-c',
            '-f', tarf, '.']).check_returncode()

if __name__ == '__main__':
  parser = argparse.ArgumentParser(
    description='gen - synthesizes dedup benchmark',
    usage="""
      -- Linux source tree benchmark --

      The 'schedule' subcommand creates a list of commit hashes
      of the Linux kernel repository (provided with --linux).
      Schedule parameters can be adjusted with --months and
      --days_per_month.

      The 'mktars' command will take the schedule specified in
      --schedule and create tar archives for each of these
      commits.

      Example use:
        gen.py --out schedule.txt --linux=../linux schedule
        gen.py --out bench/linux --schedule schedule.txt mktars
    """)
  parser.add_argument('--linux', default='../linux')
  parser.add_argument('--months', default=3, type=int)
  parser.add_argument('--days_per_month', default=15, type=int)
  parser.add_argument('--schedule', default='schedule.txt')
  parser.add_argument('--out', required=True)
  parser.add_argument('action')
  args = parser.parse_args()

  try:
    {
      'schedule': schedule,
      'mktars': mktars,
    }[args.action](args)
  except KeyError as k:
    print(f"unknown command {k}")
