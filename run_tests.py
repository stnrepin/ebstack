import time
import subprocess

MAX_THREAD = 200
ATTEMPTS_PER_THREAD = 2

EBS_EXE = './build/ebs'
MTX_EXE = './build/mtx'

def run_test(exe, th):
    cmd = f'{exe} {th}'
    r = subprocess.run(cmd, shell=True, check=True, capture_output=True, text=True)
    _, t = r.stdout.split(' ')
    return int(t[:-1])


def run_tests(res, exe, name):
    for th in range(2, MAX_THREAD+1):
        res[name][th] = []
        for i in range(0, ATTEMPTS_PER_THREAD):
            r = run_test(exe, th)
            res[name][th].append(r);
            time.sleep(1)

        print(f'{name} thread={th} res={res[name][th]}')
    print()

def calc_mean(res, name):
    for th in range(2, MAX_THREAD+1):
        res[name][f'{th}_mean'] = sum(res[name][th]) / ATTEMPTS_PER_THREAD

def main():
    res = { 'ebs': {}, 'mtx': {} }

    run_tests(res, EBS_EXE, 'ebs')
    run_tests(res, MTX_EXE, 'mtx')

    calc_mean(res, 'ebs')
    calc_mean(res, 'mtx')

    print(res)

if __name__ == '__main__':
    main()
