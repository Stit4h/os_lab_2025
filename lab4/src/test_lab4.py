#!/usr/bin/env python3
"""Functional tests; all generated files live in TemporaryDirectory."""
import ctypes
import os
from pathlib import Path
import re
import signal
import subprocess
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parent
os.chdir(ROOT)
libc = ctypes.CDLL(None)
libc.srand.argtypes = [ctypes.c_uint]
libc.rand.restype = ctypes.c_int
checks = 0

def run(name, *args, env=None):
    return subprocess.run([str(ROOT / name), *map(str, args)], text=True,
                          capture_output=True, timeout=15, env=env)

def expect(condition, message):
    global checks
    assert condition, message
    checks += 1
    print('PASS', message, flush=True)

def reference(seed, size):
    libc.srand(seed)
    values = [libc.rand() for _ in range(size)]
    return min(values), max(values), sum(values)

def timeout_demo(by_files=False):
    # Stop only this program's forked children, before they can finish.
    # The production binary and its timeout logic remain unchanged.
    helper = r'''
#define _GNU_SOURCE
#include <dlfcn.h>
#include <signal.h>
#include <sys/types.h>
#include <unistd.h>
pid_t fork(void) {
  pid_t (*real_fork)(void) = (pid_t (*)(void))dlsym(RTLD_NEXT, "fork");
  pid_t pid = real_fork();
  if (pid == 0) raise(SIGSTOP);
  return pid;
}
'''
    with tempfile.TemporaryDirectory(prefix='lab4-timeout-') as temp:
        source = Path(temp) / 'stop_children.c'
        library = Path(temp) / 'stop_children.so'
        source.write_text(helper)
        subprocess.run(['gcc', '-shared', '-fPIC', str(source), '-o', str(library), '-ldl'], check=True)
        env = dict(os.environ, LD_PRELOAD=str(library))
        args = ['--seed', 42, '--array_size', 100003, '--pnum', 4, '--timeout', 1]
        if by_files:
            args += ['--by_files']
        start = time.monotonic()
        result = run('parallel_min_max', *args, env=env)
        elapsed = time.monotonic() - start
    print('Controlled timeout test: child processes paused with SIGSTOP', flush=True)
    print('Program: ./parallel_min_max --seed 42 --array_size 100003 --pnum 4 --timeout 1'
          + (' --by_files' if by_files else ''))
    print(result.stderr, end='')
    print(f'Exit code: {result.returncode}; wall time: {elapsed:.3f}s')
    expect(result.returncode == 124, 'timeout returns 124')
    killed = re.findall(r'Child (\d+): signal 9, reaped', result.stderr)
    expect(len(killed) == 4, 'all four children killed by SIGKILL and reaped')
    expect(not result.stdout.strip(), 'no incomplete min/max result printed')
    expect(0.8 < elapsed < 5, 'timeout completes near one second')
    for pid_text in killed:
        expect(not Path('/proc', pid_text).exists(), f'child {pid_text} no longer exists')
    expect(not list(ROOT.glob('minmax_*.tmp')), 'no temporary result files remain')

def full_tests():
    for seed, size, count in [(42, 100003, 4), (1, 1, 1), (7, 17, 6)]:
        low, high, total = reference(seed, size)
        for files in (False, True):
            args = ['--seed', seed, '--array_size', size, '--pnum', count]
            if files:
                args += ['--by_files']
            for limit in ([], ['--timeout', 10]):
                result = run('parallel_min_max', *args, *limit)
                expect(result.returncode == 0 and f'Min: {low}\n' in result.stdout
                       and f'Max: {high}\n' in result.stdout,
                       f'min/max seed={seed} N={size} p={count} files={files} timeout={bool(limit)}')
        for threads in sorted(set([1, count, min(size, 8)])):
            result = run('psum', '--seed', seed, '--array_size', size, '--threads_num', threads)
            expect(result.returncode == 0 and f'Total: {total}\n' in result.stdout,
                   f'sum seed={seed} N={size} threads={threads}: {total}')
    result = run('psum', '--seed', 0, '--array_size', 11, '--threads_num', 3)
    expect(result.returncode == 0 and f'Total: {reference(0,11)[2]}\n' in result.stdout, 'seed zero accepted by psum')
    for name, args in [
        ('psum', ['--threads_num', 0, '--seed', 42, '--array_size', 10]),
        ('psum', ['--threads_num', 11, '--seed', 42, '--array_size', 10]),
        ('psum', ['--threads_num', 2, '--seed', '-1', '--array_size', 10]),
        ('psum', ['--threads_num', 2, '--seed', '4abc', '--array_size', 10]),
        ('psum', ['--threads_num', 2, '--array_size', 10]),
        ('psum', ['--threads_num', 2, '--seed', 42, '--array_size', '999999999999999999999']),
        ('parallel_min_max', ['--seed', 42, '--array_size', 10, '--pnum', 2, '--timeout', 0]),
        ('parallel_min_max', ['--seed', 42, '--array_size', 10, '--pnum', 2, '--timeout', '-1']),
        ('parallel_min_max', ['--seed', 42, '--array_size', 10, '--pnum', 11]),
        ('parallel_min_max', ['--seed', 42, '--array_size', 10, '--pnum', '2abc'])]:
        expect(run(name, *args).returncode != 0, f'invalid arguments rejected: {name} {args}')
    zombie = run('zombie', 0)
    expect(zombie.returncode == 0 and re.search(r'\bZ\S*\s+zombie', zombie.stdout)
           and 'ECHILD (no zombie remains)' in zombie.stdout, 'zombie visible as Z and removed by waitpid')
    memory = run('process_memory')
    expect(memory.returncode == 0 and len(re.findall(r'0x[0-9a-f]+', memory.stdout)) == 9,
           'process_memory prints nine complete pointer addresses')
    timeout_demo(False)
    timeout_demo(True)
    print(f'ALL {checks} CHECKS PASSED')

if __name__ == '__main__':
    if '--timeout-demo' in sys.argv:
        timeout_demo('--by-files' in sys.argv)
    else:
        full_tests()
