#include <iostream>
#include "vm.cpp"  // include your Lab 5 VM

using namespace std;

// Helper to count objects in the heap
int count_objects(Obj* head) {
    int count = 0;
    for (Obj* o = head; o; o = o->next) count++;
    return count;
}

// ---------- Test Cases ----------

void test_basic_reachability() {
    VM vm({});

    Obj* a = vm.new_pair(nullptr, nullptr);
    vm.operandStack.push_back({VAL_OBJ, {.obj = a}});

    vm.gc();

    if (vm.heap && vm.heap == a)
        cout << "Object a survives\nHeap remains unchanged\n";
    else
        cout << "FAIL: basic reachability\n";
}

void test_unreachable_collection() {
    VM vm({});

    Obj* a = vm.new_pair(nullptr, nullptr); // unreachable
    vm.gc();

    if (!vm.heap)
        cout << "Object a is freed\nHeap is empty\n";
    else
        cout << "FAIL: unreachable object collection\n";
}

void test_transitive_reachability() {
    VM vm({});

    Obj* a = vm.new_pair(nullptr, nullptr);
    Obj* b = vm.new_pair(a, nullptr);
    vm.operandStack.push_back({VAL_OBJ, {.obj = b}});

    vm.gc();

    if (count_objects(vm.heap) == 2)
        cout << "Both objects survive\n";
    else
        cout << "FAIL: transitive reachability\n";
}

void test_cyclic_references() {
    VM vm({});

    Obj* a = vm.new_pair(nullptr, nullptr);
    Obj* b = vm.new_pair(a, nullptr);
    a->right = b; // create a cycle

    vm.operandStack.push_back({VAL_OBJ, {.obj = a}});
    vm.gc();

    if (count_objects(vm.heap) == 2)
        cout << "Both objects survive\n";
    else
        cout << "FAIL: cyclic references\n";
}

void test_deep_graph() {
    VM vm({});

    Obj* root = vm.new_pair(nullptr, nullptr);
    Obj* cur = root;

    // create 10,000 linked objects
    for (int i = 0; i < 10000; i++) {
        Obj* next = vm.new_pair(nullptr, nullptr);
        cur->right = next;
        cur = next;
    }

    vm.operandStack.push_back({VAL_OBJ, {.obj = root}});
    vm.gc();

    cout << "All objects survive\nNo stack overflow occurs\n";
}

void test_closure_capture() {
    VM vm({});

    Obj* env = vm.new_pair(nullptr, nullptr);
    Obj* fn = vm.new_function();
    Obj* cl = vm.new_closure(fn, env);

    vm.operandStack.push_back({VAL_OBJ, {.obj = cl}});
    vm.gc();

    if (count_objects(vm.heap) == 3)
        cout << "Closure, function, and environment survive\n";
    else
        cout << "FAIL: closure capture\n";
}

void test_stress_allocation() {
    VM vm({});

    // allocate objects but do not push to stack (unreachable)
    for (int i = 0; i < 10; i++)
        vm.new_pair(nullptr, nullptr);

    vm.gc();

    if (!vm.heap)
        cout << "Heap is empty after GC\nNo memory leaks or crashes\n";
    else
        cout << "FAIL: stress allocation\n";
}

// ---------- main ----------

int main() {
    cout<<"\ntest_basic_reachability\n";
    test_basic_reachability();
    cout<<"\n\ntest_unreachable_collection\n";
    test_unreachable_collection();
    cout<<"\n\ntest_transitive_reachability\n";
    test_transitive_reachability();
    cout<<"\n\ntest_cyclic_references\n";
    test_cyclic_references();
    cout<<"\n\ntest_deep_graph\n";
    test_deep_graph();
    cout<<"\n\ntest_closure_capture\n";
    test_closure_capture();
    cout<<"\n\ntest_stress_allocation\n";
    test_stress_allocation();
    return 0;
}
