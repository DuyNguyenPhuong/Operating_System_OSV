#!/usr/bin/python3

import argparse
import os
import sys
import time
import json
import subprocess
import shutil
from select import select
from subprocess import Popen, TimeoutExpired

# The number of seconds it takes to run the test suite
TIMEOUT = {
    1: 60,
    2: 60,
    3: 60,
    4: 60,
    5: 60,
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
    "2-fork-fd": 15,
    "2-fork-test": 15,
    "2-fork-tree": 15,
    "2-race-test": 10,
    "2-wait-twice": 5,
    "2-wait-bad-args": 15,
    "2-exit-test": 15,
    "3-pipe-basic": 10,
    "3-pipe-bad-args": 10,
    "3-pipe-errors": 10,
    "3-pipe-test": 20,
    "3-pipe-robust": 20,
    "3-pipe-race": 20,
    "4-bad-mem-access": 10,
    "4-grow-stack": 15,
    "4-grow-stack-edgecase": 10,
    "4-malloc-test": 10,
    "4-sbrk-decrement": 15,
    "4-sbrk-large": 15,
    "4-sbrk-small": 15,
    "5-cow-small": 10,
    "5-cow-large": 14,
    "5-cow-multiple": 20,
    "5-cow-low-mem": 25,
    "5-REDO-4": 21
}

autograder_root = "/autograder"
submission_files = [f"{autograder_root}/submission/arch",
                    f"{autograder_root}/submission/include",
                    f"{autograder_root}/submission/kernel"]
proc_flags = {
    "stdout": subprocess.PIPE,
    "stderr": subprocess.STDOUT,
    "universal_newlines": True
}

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
    if lab in TIMEOUT:
        results = {"tests": []}
        # check if this lab should include tests from prior labs
        for test, w in test_weights.items():
            if test.startswith(f"{lab}-REDO"):
                subscore = run_tests(int(test[-1]), autograder)
                score += subscore / 90 * w
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
    return score


def main():
    parser = argparse.ArgumentParser(description="Run osv tests")
    parser.add_argument('lab_number', type=int, help='lab number')
    parser.add_argument('--autograder', help="produce autograder output for Gradescope")
    args = parser.parse_args()

    if args.autograder:
        # check for correct submission directories
        if not all(os.path.exists(f) for f in submission_files):
            result["score"] = 0
            result["output"] = "\n".join(f"{f.split('/')[-1]} directory not found at {f} in submission" for f in submission_files
                if not os.path.exists(f))
            with open(f"{autograder_root}/results/results.json", 'w') as fp:
                json.dump(result, fp)
            sys.exit(1)

        # setup
        for f in submission_files:
            subprocess.run(["cp", "-r", f, f.replace("submission", "source")])
        os.chdir(f"{autograder_root}/source/")
        subprocess.run(["chmod", "+x", "arch/x86_64/boot/sign.py"])
        shutil.rmtree("build", ignore_errors=True)
        result["tests"] = []

        # build
        try:
            stdout = ""
            make = subprocess.run(["make"], check=True, **proc_flags)
            stdout += make.stdout
            result["tests"].append(make_test_result(0, 0, "build", stdout))
        except subprocess.CalledProcessError as err:
            result["tests"].append(make_test_result(
                0, 0, "build", stdout + err.stdout))
            # add feedback that other tests were not run due to compilation error
            result["tests"].append(make_test_result(
                    0, 90, "correctness tests", "submission did not compile, tests not run"))
            with open(f"{autograder_root}/results/results.json", 'w') as fp:
                json.dump(result, fp)
            sys.exit(2)

    run_tests(args.lab_number, args.autograder)

def run_tests(lab_number, autograder):
    result = {}
    test_stats = {}
    lab = lab_number
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
            if "low-mem" in test:
                qemu = Popen(["make", "qemu-test-low-mem", "--quiet"])
            else:
                qemu = Popen(["make", "qemu-test", "--quiet"])
            pin = open(f"build/osv-test.in", "w")
            pout = open(f"build/osv-test.out")
            print("booting osv")
            select([pout], [], [])
            # select seems to return slightly before osv has finished booting
            time.sleep(0.5)
            print(f"sending {test}\n")
            pin.write(f"{test}\n")
            pin.flush()
            time.sleep(0.2)
            try:
                print(f"sending quit\n")
                pin.write(f"quit\n")
                pin.flush()
            except BrokenPipeError:
                # ok if qemu has already quit
                pass
            try:
                print("waiting for osv")
                qemu.wait(timeout=TIMEOUT[lab])
                print("reading output")
                output = pout.read()
                out.write(output)
                test_stdout[test] = output
            except TimeoutExpired as e:
                test_stdout[test] = f"Exceeded Timeout {TIMEOUT[lab]} seconds\n\
                possibly due to kernel panic, check contents of lab{lab}output file"
                print(test_stdout[test])
                output = ""
                qemu.terminate()
                pass
            finally:
                try:
                    pin.close()
                    pout.close()
                except BrokenPipeError:
                    # ok if already closed
                    pass

            # read output from test
            if check_output(out, test, ofs):
                print(ANSI_GREEN + "passed " + ANSI_RESET + test)
                test_stats[test] = PASSED
            else:
                test_stats[test] = FAILED
                print(ANSI_RED + "failed " + ANSI_RESET + test)
            print('-------------------------------')
        except BrokenPipeError:
            # This just means that QEMU never started
            test_stats[test] = FAILED
            test_stdout[test] = "failed to start qemu"
            print(test_stdout[test])

    # examine test stats
    score = test_summary(test_stats, lab, test_stdout, autograder)
    out.close()
    return score


if __name__ == "__main__":
    main()
