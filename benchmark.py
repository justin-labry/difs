import csv
import shutil
import signal
import tempfile

from time import sleep
from datetime import datetime
from multiprocessing.pool import ThreadPool
from subprocess import DEVNULL, Popen, TimeoutExpired, call
from pprint import pprint


CONCURRENCY = 10
REPEAT = 10
REPO_DIR = '/var/lib/ndn/repo/0'


def reset_repo():
    print('Resetting repo')
    try:
        call(['killall', 'nfd', 'ndn-repo-ng'])
        shutil.rmtree(REPO_DIR)
    except:
        pass

def run_get(id_):
    start = datetime.now()
    p = Popen(
        ['build/tools/ndngetfile', '/example/data/1'],
        stdout=DEVNULL, stderr=DEVNULL)
    code = p.wait()
    end = datetime.now()
    delta = end - start
    return (id_, code, delta.total_seconds())


def run_test(size):
    reset_repo()
    p_nfd, p_repo = None, None

    def restart_repo(p_nfd, p_repo):
        print('Restarting repo')
        if p_repo:
            p_repo.send_signal(signal.SIGINT)
            try:
                p_repo.wait(1)
            except TimeoutExpired:
                p_repo.kill()

        if p_nfd:
            p_nfd.send_signal(signal.SIGINT)
            try:
                p_nfd.wait(1)
            except TimeoutExpired:
                p_nfd.kill()

        p_nfd = Popen(['nfd'], stdout=DEVNULL, stderr=DEVNULL)
        sleep(5)

        p_repo = Popen(['build/ndn-repo-ng', '-c', 'repo-0.conf'], stdout=DEVNULL)
        sleep(5)

        return p_nfd, p_repo

    with tempfile.NamedTemporaryFile() as tfile:
        print(f'Testing with {size}')

        p_nfd, p_repo = restart_repo(p_nfd, p_repo)

        call([
            'fallocate', '-l', str(size), tfile.name
        ])

        code = call([
            'build/tools/ndnputfile',
            '/example/repo',
            '/example/data/1',
            tfile.name
        ])

        print(f'return code: {code}')

        print(f'Putfile {size} done')

        for iteration in range(REPEAT):
            print(f'Iteration {iteration}')

            p_nfd, p_repo = restart_repo(p_nfd, p_repo)

            pool = ThreadPool(CONCURRENCY)
            results = pool.map(run_get, range(CONCURRENCY))
            yield [
                (size, iteration, *values)
                for values in results
            ]

        p_nfd.kill()
        p_repo.kill()

sizes = [2 ** i for i in range(10, 30 + 1)]

with open('results.tsv', 'w') as tsvfile:
    writer = csv.writer(tsvfile, delimiter='\t')
    for size in sizes:
        for row in run_test(size):
            pprint(row)
            writer.writerows(row)
            tsvfile.flush()
