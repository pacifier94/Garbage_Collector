# Lab 5 – Mark-Sweep Garbage Collector

This project extends the Virtual Machine (VM) from Lab 4 by integrating a stop-the-world Mark–Sweep Garbage Collector. The VM now supports heap allocation, explicit garbage collection, and safe memory reclamation for dynamically allocated objects such as pairs, functions, and closures.

---

## Project Structure

```

.
├── vm.cpp          # Virtual Machine with integrated Mark–Sweep GC
├── asm.c           # Two-pass assembler
├── fib.asm         # Sample program (Fibonacci)
├── gc_tests.cpp    # Garbage Collector test cases
├── Makefile.sh     # Build file
├── program.bin     # Output bytecode (generated)

````

---

## Requirements

- GCC / G++ compiler
- Linux or macOS
- Make utility

---

## Build Instructions

Rename the build file (recommended):

```bash
mv Makefile.sh Makefile
````

Build everything:

```bash
make
```

This generates:

* `asm` → assembler
* `vm`  → virtual machine

---

## Assembling a Program

```bash
./asm fib.asm program.bin
```

This creates `program.bin`.

---

## Running the Virtual Machine

```bash
./vm program.bin
```

Expected output for Fibonacci:

```
Final Result: 55
```

---

## Triggering Garbage Collection

Garbage collection is explicitly triggered using the `GC` opcode inside the program. When executed, the VM:

1. Pauses execution
2. Performs mark–sweep GC
3. Prints statistics
4. Resumes execution

Example:

```
[GC] Collected: 5 objects | Remaining: 2 | Time: 12 us
```

---

## Running Garbage Collector Tests

Compile GC tests:

```bash
g++ -DGC_TEST gc_tests.cpp -o gc_tests
```

Run:

```bash
./gc_tests
```

These tests validate:

* Reachable vs unreachable objects
* Transitive reachability
* Cyclic references
* Deep object graphs
* Closure capture
* Stress allocation and cleanup

---

## Garbage Collector Design

* Type: Stop-the-world Mark–Sweep
* Heap: Linked list of allocated objects
* Roots:

  * Operand stack
  * VM memory array
* Object types:

  * Pair
  * Function
  * Closure

GC Phases:

1. **Mark Phase** – Traverses and marks reachable objects
2. **Sweep Phase** – Frees unmarked objects

---

## Key Features

* Explicit garbage collection trigger
* Correct handling of cyclic references
* Deep object graph support
* Closure environment preservation
* No memory leaks or unsafe access

---

## Notes

* GC is stop-the-world
* No heap compaction
* No generational or incremental collection

---

## Author

Developed as part of a Systems Concepts laboratory assignment. by- Astitwa Saxena 2025MCS3005 Abhishek Gupta 2025MCS2963
```

```
```
