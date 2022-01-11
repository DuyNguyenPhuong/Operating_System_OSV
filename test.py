#!/usr/bin/python3

import argparse
import os
import time
import json
from select import select
from subprocess import Popen, TimeoutExpired

# The number of seconds it takes to run the test suite
TIMEOUT = {
    1: 60,
    3: 60,
    4: 60,
}

test_weights = {
    "1-close-test": 10,
    "1-dup-console": 7,
    "1-dup-read": 7,
    "1-fd-limit": 5,
    "1-fstat-test": 7,
    "1-open-bad-args": 10,
    "1-open-twice": 10,
    "1-read-bad-args": 10,
    "1-read-small": 10,
    "1-readdir-test": 7,
    "1-write-bad-args": 7,
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

autograder_root = "/autograder"

# ANSI color
ANSI_RED = '\033[31m'
ANSI_GREEN = '\033[32m'
ANSI_RESET = '\033[0m'

# test state
PASSED = 1
FAILED = 0


def make_test_result(score: float, max_score: float, name: str, output: str) -> dict:
    return {
        "score": score,
        "max_score": max_score,
        "name": name,
        "output": output
    }

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


def test_summary(test_stats, lab, outputs, autograder):
    score = 0
    if lab == 1 or lab == 3 or lab == 4:
        results = {"tests": []}
        for test, result in test_stats.items():
            if f"{lab}-{test}" in test_weights:
                w = test_weights[f"{lab}-{test}"]
                results["tests"].append(make_test_result(w if result == PASSED else 0, w, test, outputs[test]))
                if result == PASSED:
                    score += w
        if autograder:
            with open(f"{autograder_root}/results/results.json", 'w') as fp:
                json.dump(result, fp)
    else:
        print(f"lab{lab} tests not available")
    print(f"lab{lab} test score: {score}/90")


def main():
    parser = argparse.ArgumentParser(description="Run osv tests")
    parser.add_argument('lab_number', type=int, help='lab number')
    parser.add_argument('--autograder', help="produce autograder output for Gradescope")
    args = parser.parse_args()

    test_stats = {}
    lab = args.lab_number
    out = open("lab"+str(lab)+"output", "w+")
    test_stdout = {}

    # retrieve list of tests for the lab
    testdir = os.path.join(os.getcwd(), "user/lab"+str(lab))
    out.write("test dir: "+testdir+"\n")
    try:
        os.mkdir("build")
    except FileExistsError:
        pass
    except Exception:
        raise        
    try:
        os.mkfifo(f"build/osv-test.in")
    except FileExistsError:
        pass
    except Exception:
        raise
    try:
        os.mkfifo(f"build/osv-test.out")
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
            pin = open(f"build/osv-test.in", "w")
            pout = open(f"build/osv-test.out")
            print("booting osv")
            select([pout], [], [])
            # select seems to return slightly before osv has finished booting
            time.sleep(0.5)
            print(f"sending {test}\nquit\n")
            pin.write(f"{test}\nquit\n")
            pin.flush()
            try:
                print("waiting for osv")
                qemu.wait(timeout=TIMEOUT[lab])
                print("reading output")
                output = pout.read()
                out.write(output)
                test_stdout[test] = output
            except TimeoutExpired as e:
                print("Exceeded Timeout " + str(TIMEOUT[lab]) + " seconds")
                print(f"possibly due to kernel panic, check contents of lab{lab}output file")
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
    test_summary(test_stats, lab, test_stdout, args.autograder)
    out.close()


if __name__ == "__main__":
    main()
