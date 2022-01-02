#!/usr/bin/python3

import argparse
import os
import subprocess
import sys
import os
import time
from select import select
from subprocess import Popen, PIPE, STDOUT, TimeoutExpired

# The number of seconds it takes to run the test suite
TIMEOUT = {
    2: 60,
    3: 60,
    4: 60,
}

test_weights = {
    "2-close-test": 18,
    "2-dup-console": 2,
    "2-dup-read": 2,
    "2-fd-limit": 3,
    "2-fstat-test": 2,
    "2-open-bad-args": 12,
    "2-open-twice": 12,
    "2-read-bad-args": 12,
    "2-read-small": 18,
    "2-readdir-test": 2,
    "2-write-bad-args": 2,
    "3-fork-fd": 25,
    "3-fork-test": 25,
    "3-fork-tree": 25,
    "3-pipe-robust": 0,
    "3-pipe-test": 0,
    "3-race-test": 10,
    "3-spawn-args": 0,
    "3-wait-twice": 15,
    "4-bad-mem-access": 10,
    "4-grow-stack": 25,
    "4-grow-stack-edgecase": 10,
    "4-malloc-test": 10,
    "4-sbrk-decrement": 15,
    "4-sbrk-large": 15,
    "4-sbrk-small": 15
}

# ANSI color
ANSI_RED = '\033[31m'
ANSI_GREEN = '\033[32m'
ANSI_RESET = '\033[0m'

# test state
PASSED = 1
FAILED = 0


def check_output(out, test, ofs):
    error = False
    test_passed = False
    out.seek(ofs)
    # read from ofs to EOF
    for line in out:
        if ANSI_GREEN+"passed "+ANSI_RESET+test in line:
            test_passed = True
        if "ERROR" in line or "Assertion failed" in line or "PANIC" in line:
            error = True  # can't break here, need to finish reading to EOF
    return test_passed and not error


def test_summary(test_stats, lab):
    score = 0
    if lab == 2 or lab == 3 or lab == 4:
        for test, result in test_stats.items():
            if result == PASSED:
                if "{}-{}".format(lab, test) in test_weights:
                    score += test_weights["{}-{}".format(lab, test)]
    else:
        print("lab{} tests not available".format(lab))
    print("lab{0}test score: {1}/100".format(lab, score))


def main():
    parser = argparse.ArgumentParser(description="Run osv tests")
    parser.add_argument('lab_number', type=int, help='lab number')
    args = parser.parse_args()

    test_stats = {}
    lab = args.lab_number
    out = open("lab"+str(lab)+"output", "w+")

    # retrieve list of tests for the lab
    testdir = os.path.join(os.getcwd(), "user/lab"+str(lab))
    out.write("test dir: "+testdir+"\n")
    try:
        os.mkfifo("/tmp/osv-test.in")
    except FileExistsError:
        pass
    except Exception:
        raise
    try:
        os.mkfifo("/tmp/osv-test.out")
    except FileExistsError:
        pass
    except Exception:
        raise

    for test in os.listdir(testdir):
        if test.endswith(".c"):
            test = test[:-2]
            out.write("running test: "+test+"\n")
            print("running test: "+test)

        # found test, run in a subprocess
        try:
            ofs = out.tell()
            qemu = Popen(["make", "qemu-test", "--quiet"])
            pin = open("/tmp/osv-test.in", "w")
            pout = open("/tmp/osv-test.out")
            print("booting osv")
            select([pout], [], [])
            time.sleep(0.5) # select seems to return slightly before osv has finished booting
            print("sending {}\nquit\n".format(test))
            pin.write("{}\nquit\n".format(test))
            pin.flush()
            try:
                print("waiting for osv")
                qemu.wait(timeout=TIMEOUT[lab])
                print("reading output")
                out.write(pout.read())
            except TimeoutExpired as e:
                print("Exceeded Timeout " + str(TIMEOUT[lab]) + " seconds")
                print("possibly due to kernel panic, check lab{}output".format(lab))
                qemu.terminate()
                pass
            finally:
                pin.close()
                pout.close()

            # read output from test
            if check_output(out, test, ofs):
                print(ANSI_GREEN + "passed " + ANSI_RESET + test)
                test_stats[test] = PASSED
            else:
                test_stats[test] = FAILED
                print(ANSI_RED + "failed " + ANSI_RESET + test)
            print('-------------------------------')
        except BrokenPipeError:
            print("fails to start qemu")
            # This just means that QEMU never started
            pass

    # examine test stats
    test_summary(test_stats, lab)
    out.close()


if __name__ == "__main__":
    main()
