import subprocess
import random
import os

BUFFER_EXE = "./buffer"
REFERENCE_EXE = "./buffer_BUSY_NOBARGINGCHECK"
BIMPL = ["BUSY"]


def write_to_file(content, filename):
    """Write content to a file."""
    with open(filename, "w") as f:
        f.write(content)


def build_buffer(bimpl):
    print(f"\n🔧 Building buffer with BIMPL={bimpl}...\n")
    bcheck = "BARGINGCHECK" if bimpl == "NOBUSY" else "NOBARGINGCHECK"
    result = subprocess.run(
        ["make", "buffer", f"BIMPL={bimpl}", f"BCHECK={bcheck}"], capture_output=True, text=True)
    if result.returncode != 0:
        print(f"❌ Build failed for {bimpl}:\n{result.stderr}")
        exit(1)
    print(f"✅ Successfully built buffer with {bimpl}\n")


def generate_random_params():
    cons = random.randint(5, 100)       # Number of consumers
    prods = random.randint(5, 100)      # Number of producers
    produce = random.randint(10, 4000)  # Items produced per producer
    buf_size = random.randint(5, 50)   # Buffer size
    delay = random.randint(1, 3)      # Delay for yielding
    procs = random.randint(1, 8)       # Number of processors
    return cons, prods, produce, buf_size, delay, procs


def run_test(cons, prods, produce, buf_size, delay, procs, mode):
    cmd = [BUFFER_EXE, str(cons), str(prods), str(produce), str(buf_size), str(delay), str(procs)]
    print(f"▶ Running {mode} buffer test: {' '.join(cmd)}")

    # Run buffer executable
    result = subprocess.run(cmd, capture_output=True, text=True)

    # Capture stdout and stderr
    output = result.stdout.strip()
    error = result.stderr.strip()

    return output, error


def run_reference(cons, prods, produce, buf_size, delay, procs):
    cmd = [REFERENCE_EXE, str(cons), str(prods), str(
        produce), str(buf_size), str(delay), str(procs)]
    result = subprocess.run(cmd, capture_output=True, text=True)
    return result.stdout.strip(), result.stderr.strip()


def validate_tests(test_count=10):
    for bimpl in BIMPL:
        build_buffer(bimpl)

        passed_tests = 0
        failed_tests = 0

        for i in range(test_count):
            cons, prods, produce, buf_size, delay, procs = generate_random_params()
            args = f"Cons={cons}, Prods={prods}, Produce={produce}, BufferSize={buf_size}, Delay={delay}, Processors={procs}"
            print(f"\n🔍 Test {i+1}/{test_count}: {args}")

            # Run BUSY version
            output, error = run_test(cons, prods, produce, buf_size, delay, procs, bimpl)

            # Run reference implementation
            reference_output, _ = run_reference(cons, prods, produce, buf_size, delay, procs)

            write_to_file(args, f"test_results/{bimpl}_args_{i+1}.txt")
            write_to_file(output, f"test_results/{bimpl}_output_{i+1}.txt")
            write_to_file(reference_output, f"test_results/{bimpl}_reference_{i+1}.txt")

            # Validate output (must match reference)
            if output != reference_output:
                print(f"❌ Mismatch in output for test case (Cons={cons}, Prods={prods}, ...)")
                failed_tests += 1
                continue

            if bimpl == "NOBUSY":
                # Validate error (must be empty for NOBUSY)
                if "**** BARGING ERROR ****" in error:
                    print(f"❌ Error in output for test case (Cons={cons}, Prods={prods}, ...)")
                    write_to_file(error, f"test_results/{bimpl}_error_{i+1}.txt")
                    failed_tests += 1
                    continue

            print("✅ Test passed!")
            passed_tests += 1

        print(f"\n🎯 Total Passed: {passed_tests}, Total Failed: {failed_tests}")
        if failed_tests == 0:
            print("🎉 All tests passed successfully!")
        else:
            print("⚠️ Some tests failed. Check logs for details.")


# Run the validation tests
if __name__ == "__main__":
    # Create a directory for test cases
    os.makedirs("test_results", exist_ok=True)
    validate_tests(test_count=100)  # Run 20 randomized tests