#!/usr/bin/python3
import argparse
import os
import sys
import subprocess as sp

CHUNKERS = ['g0', 'g1']

def mkdir(d):
  try:
    os.makedirs(d)
  except FileExistsError:
    pass

def run(args):
  category = args.category
  if category[-1] == '/':
    category = category[0:-1]
  results = {}
  catd = os.path.join('bench', 'results', os.path.basename(category))
  for chunker in CHUNKERS:
    resd = os.path.join(catd, chunker)
    allf = os.path.join(resd, 'all.sums')
    if args.f or not os.path.exists(allf):
      sp.run(f'rm -fr {resd}/*', shell=True).check_returncode()
      mkdir(resd)
      for file in os.listdir(category):
        datf = os.path.join(category, file)
        resf = os.path.join(resd, file + '.chunks')
        sumf = os.path.join(resd, file + '.sums')
        print(f'running {chunker} on {datf}')
        sp.run(f'./{chunker} {datf} > {resf}', shell=True).check_returncode()
        sp.run(f'./sums {resf} {datf} > {sumf}', shell=True).check_returncode()
    sp.run(f'cat {resd}/*.sums | sort | uniq -c > {allf}', shell=True).check_returncode()

    print(f"processing results for {chunker}...")
    maxchunk = 0
    minchunk = 1 << 40
    nchunks = 0
    totalsz = 0
    dedupsz = 0
    with open(allf, 'r') as f:
      for line in f.readlines():
        (cnt, hash, len) = line.split()
        cnt = int(cnt)
        len = int(len)
        totalsz += len * cnt
        dedupsz += len
        nchunks += 1
        if len < minchunk:
          minchunk = len
        if len > maxchunk:
          maxchunk = len
    results[chunker] = {
        'minchunk': minchunk,
        'maxchunk': maxchunk,
        'avgchunk': (1.0 * dedupsz) / nchunks,
        'totalsz': totalsz,
        'dedupsz': dedupsz,
        'deduprate': 100.0 - (100.0 * dedupsz) / totalsz,
      }

  with open(os.path.join(catd, 'results.txt'), 'w') as resf:
    for chunker in results:
      resf.write(f'* {chunker}\n')
      for (stat, value) in results[chunker].items():
        resf.write(f'  {stat}: {value}\n')

def usage():
  print('usage: bench/run.py [ run ] ARGS')
  exit(1)

if __name__ == '__main__':
  if len(sys.argv) < 2:
    usage()
  if sys.argv[1] == 'run':
    ap = argparse.ArgumentParser(prog='bench/run.py run')
    ap.add_argument('-f', action='store_true')
    ap.add_argument('category')
    args = ap.parse_args(sys.argv[2:])
    run(args)
  else:
    print(f'unknown command {sys.argv[1]}')
