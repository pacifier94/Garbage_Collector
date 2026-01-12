#include <iostream>
#include <vector>
#include <fstream>
#include <cstdint>
#include <chrono>

using namespace std;
using namespace std::chrono;

enum ObjType { OBJ_PAIR, OBJ_FUNCTION, OBJ_CLOSURE };

struct Obj {
    ObjType type;
    bool marked;
    Obj* next;

    Obj* left;
    Obj* right;
    Obj* function;
    Obj* env;
};

enum ValueType { VAL_INT, VAL_OBJ };

struct Value {
    ValueType type;
    union {
        int32_t i;
        Obj* obj;
    };
};

class VM {
public:
    vector<uint8_t> code;
    vector<Value> operandStack;
    vector<uint32_t> callStack;

    Value memory[1024];

    // Heap for GC
    Obj* heap = nullptr;
    size_t numObjects = 0;

    uint32_t pc = 0;
    bool running = true;
    bool debug = false;

    VM(vector<uint8_t> bytecode) : code(bytecode) {
        reset();
    }

    void reset() {
        operandStack.clear();
        callStack.clear();
        for (int i = 0; i < 1024; i++) {
            memory[i].type = VAL_INT;
            memory[i].i = 0;
        }
        pc = 0;
        running = true;
    }

    /* =======================
       Heap Allocation
       ======================= */

    Obj* allocObject(ObjType type) {
        Obj* obj = new Obj();
        obj->type = type;
        obj->marked = false;
        obj->next = heap;
        heap = obj;
        numObjects++;

        obj->left = obj->right = nullptr;
        obj->function = obj->env = nullptr;

        return obj;
    }

    Obj* new_pair(Obj* a, Obj* b) {
        Obj* o = allocObject(OBJ_PAIR);
        o->left = a;
        o->right = b;
        return o;
    }

    Obj* new_function() {
        return allocObject(OBJ_FUNCTION);
    }

    Obj* new_closure(Obj* fn, Obj* env) {
        Obj* o = allocObject(OBJ_CLOSURE);
        o->function = fn;
        o->env = env;
        return o;
    }

    ~VM() {
    Obj* current = heap;
    while (current) {
        Obj* next = current->next;
        delete current;
        current = next;
    }
    heap = nullptr;
    }

    void mark(Obj* root) {
        vector<Obj*> stack;
        stack.push_back(root);

        while (!stack.empty()) {
            Obj* o = stack.back();
            stack.pop_back();

            if (!o || o->marked) continue;
            o->marked = true;

            if (o->type == OBJ_PAIR) {
                stack.push_back(o->left);
                stack.push_back(o->right);
            } else if (o->type == OBJ_CLOSURE) {
                stack.push_back(o->function);
                stack.push_back(o->env);
            }
        }
    }

    void markRoots() {
        for (auto& v : operandStack)
            if (v.type == VAL_OBJ) mark(v.obj);

        for (int i = 0; i < 1024; i++)
            if (memory[i].type == VAL_OBJ) mark(memory[i].obj);
    }

    void sweep() {
        Obj** curr = &heap;
        while (*curr) {
            if (!(*curr)->marked) {
                Obj* dead = *curr;
                *curr = dead->next;
                delete dead;
                numObjects--;
                cout << "Object freed:" <<numObjects<< endl;
            } else {
                (*curr)->marked = false;
                curr = &(*curr)->next;
            }
        }
    }

    void gc() {
        auto start = high_resolution_clock::now();
        size_t before = numObjects;

        markRoots();
        sweep();

        auto stop = high_resolution_clock::now();
        auto duration = duration_cast<microseconds>(stop - start);

        if (debug || true) { // Set to true to always see performance in tests
            cout << "[GC] Collected: " << (before - numObjects) 
                << " objects | Remaining: " << numObjects 
                << " | Time: " << duration.count() << " us" << endl;
        }

        if (!heap) cout << "Heap is empty" << endl;
    }

    int32_t fetchInt32() {
        if (pc + 4 > code.size()) return 0;
        int32_t val =
            (static_cast<int32_t>(code[pc]) << 24) |
            (static_cast<int32_t>(code[pc+1]) << 16) |
            (static_cast<int32_t>(code[pc+2]) << 8) |
            (static_cast<int32_t>(code[pc+3]));
        pc += 4;
        return val;
    }

    Value safe_pop() {
        if (operandStack.empty()) {
            cerr << "Runtime Error: Stack Underflow" << endl;
            running = false;
            return {VAL_INT, {.i = 0}};
        }
        Value v = operandStack.back();
        operandStack.pop_back();
        return v;
    }

    void step() {
        uint8_t opcode = code[pc++];

        if (debug) {
            cout << "Executing PC: " << pc - 1
                 << " Opcode: " << (int)opcode << endl;
        }

        switch (opcode) {

            case 0x01: { // PUSH
                Value v;
                v.type = VAL_INT;
                v.i = fetchInt32();
                operandStack.push_back(v);
                break;
            }

            case 0x02: safe_pop(); break; // POP
            case 0x03: { // DUP
                if (!operandStack.empty()) operandStack.push_back(operandStack.back());
                else { cerr<<"DUP on empty stack\n"; running=false; }
                break;
            }

            case 0x10: { // ADD
                int32_t b = safe_pop().i;
                int32_t a = safe_pop().i;
                operandStack.push_back({VAL_INT,{.i=a+b}});
                break;
            }

            case 0x11: { // SUB
                int32_t b = safe_pop().i;
                int32_t a = safe_pop().i;
                operandStack.push_back({VAL_INT,{.i=a-b}});
                break;
            }

            case 0x12: { // MUL
                int32_t b = safe_pop().i;
                int32_t a = safe_pop().i;
                operandStack.push_back({VAL_INT,{.i=a*b}});
                break;
            }

            case 0x13: { // DIV
                int32_t b = safe_pop().i;
                int32_t a = safe_pop().i;
                if(b==0){ cerr<<"Division by zero\n"; running=false; break; }
                operandStack.push_back({VAL_INT,{.i=a/b}});
                break;
            }

            case 0x14: { // CMP
                int32_t b = safe_pop().i;
                int32_t a = safe_pop().i;
                int32_t r = (a<b)?-1:(a>b)?1:0;
                operandStack.push_back({VAL_INT,{.i=r}});
                break;
            }

            case 0x20: {                                             // JMP
                uint32_t addr = fetchInt32();
                if (addr < code.size()) pc = addr;
                else {
                    cerr << "Runtime Error: Invalid Jump Address" << endl;
                    running = false;
                }
                break;
            }
            //JZ
            case 0x21: { uint32_t addr=fetchInt32(); if(safe_pop().i==0) pc=addr; break; }
            //JNZ
            case 0x22: { uint32_t addr=fetchInt32(); if(safe_pop().i!=0) pc=addr; break; }

            case 0x30: {                                             // STORE
                uint32_t idx = fetchInt32();
                if (idx < 1024) {
                    if (!operandStack.empty()) {
                        memory[idx] = safe_pop();
                    } else {
                        cerr << "Runtime Error: Stack Underflow during STORE" << endl;
                        running = false;
                    }
                } else {
                    cerr << "Runtime Error: Memory Access Out of Bounds" << endl;
                    running = false;
                }
                break;
            }
            case 0x31: {                                             // LOAD
                uint32_t idx = fetchInt32();
                if (idx < 1024) {
                    operandStack.push_back(memory[idx]);
                } else {
                    cerr << "Runtime Error: Memory Access Out of Bounds" << endl;
                    running = false;
                }
                break;
            }

            case 0x40: {                                             // CALL
                uint32_t addr = fetchInt32();
                if (addr < code.size()) {
                    callStack.push_back(pc);
                    pc = addr;
                } else {
                    cerr << "Runtime Error: Invalid CALL Address" << endl;
                    running = false;
                }
                break;
            }
            case 0x41: {                                             // RET
                if (!callStack.empty()) {
                    pc = callStack.back();
                    callStack.pop_back();
                } else {
                    cerr << "Runtime Error: RET without CALL" << endl;
                    running = false;
                }
                break;
            }
            case 0x50: { Value v = safe_pop(); cout<<"OUT: "<<v.i<<endl; break; }
            case 0xFF: running=false; break;


            case 0x60: { 
            Value b = safe_pop();
            Value a = safe_pop();
            Obj* o = new_pair((a.type == VAL_OBJ) ? a.obj : nullptr,
                            (b.type == VAL_OBJ) ? b.obj : nullptr);
            operandStack.push_back({VAL_OBJ, {.obj = o}});
            break;
            }

            case 0x61: { // GC
                gc();
                cout << "[GC triggered] Heap objects: " << numObjects << endl;
                break;
            }

            case 0x62: { // PUSH_NIL
                operandStack.push_back({VAL_OBJ, {.obj = nullptr}});
                break;
            }

            case 0x70: { // NEW_FUNCTION
                Obj* fn = new_function();
                operandStack.push_back({VAL_OBJ, {.obj = fn}});
                break;
            }

            case 0x71: { // NEW_CLOSURE
                Value env = safe_pop();
                Value fn = safe_pop();
                Obj* cl = new_closure((fn.type == VAL_OBJ) ? fn.obj : nullptr,
                                    (env.type == VAL_OBJ) ? env.obj : nullptr);
                operandStack.push_back({VAL_OBJ, {.obj = cl}});
                break;
            }

            default: cerr<<"Unknown opcode "<<(int)opcode<<endl; running=false; break;
        }
    }

    void run() {
        while(running && pc<code.size()) step();
    }
};

#ifndef GC_TEST
int main(int argc, char* argv[]) {
    if (argc < 2) {
        cout << "Usage: ./vm <file.bin> [iterations]" << endl;
        return 1;
    }

    ifstream file(argv[1], ios::binary);
    if (!file) { cerr << "Could not open file." << endl; return 1; }
    vector<uint8_t> buffer((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    
    VM vm(buffer);
    vm.run();

    return 0;
}
#endif

