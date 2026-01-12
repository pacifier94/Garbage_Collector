#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

typedef struct
{
    char name[64];
    uint32_t addr;
} Label;
Label labels[256];
int label_cnt = 0;

int needs_arg(char *op)
{
    char *args[] = {"PUSH", "JMP", "JZ", "JNZ", "STORE", "LOAD", "CALL"};
    for (int i = 0; i < 7; i++)
        if (strcmp(op, args[i]) == 0)
            return 1;
    return 0;
}

void write_be32(int32_t val, FILE *f)
{
    uint8_t d[4] = {(val >> 24) & 0xff, (val >> 16) & 0xff, (val >> 8) & 0xff, val & 0xff};
    fwrite(d, 1, 4, f);
}

void check_label_overflow()
{
    if (label_cnt >= 256)
    {
        fprintf(stderr, "Error: labels limit is 256\n");
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Usage: %s <input.asm>\n", argv[0]);
        return 1;
    }

    FILE *in = fopen(argv[1], "r");
    if (!in)
    {
        perror("Failed to open input file");
        return 1;
    }

    char line[256], op[64], arg_s[64];
    uint32_t addr = 0, line_num = 0;

    // --- Pass 1: Label Collection ---
    while (fgets(line, 256, in))
    {
        line_num++;
        if (sscanf(line, "%s", op) != 1 || op[0] == ';')
            continue;

        if (op[strlen(op) - 1] == ':')
        {
            op[strlen(op) - 1] = '\0';
            for (int i = 0; i < label_cnt; i++)
            {
                if (strcmp(labels[i].name, op) == 0)
                {
                    fprintf(stderr, "Error line %u: Duplicate label '%s'\n", line_num, op);
                    return 1;
                }
            }
            check_label_overflow();
            strcpy(labels[label_cnt].name, op);
            labels[label_cnt++].addr = addr;
        }
        else
        {
            addr += needs_arg(op) ? 5 : 1;
        }
    }

    // --- Pass 2: Code Generation ---
    rewind(in);
    FILE *out = fopen("program.bin", "wb");
    if (!out)
    {
        perror("Failed to create output file");
        return 1;
    }
    line_num = 0;

    while (fgets(line, 256, in))
    {
        line_num++;
        if (sscanf(line, "%s", op) != 1 || op[0] == ';' || op[strlen(op) - 1] == ':')
            continue;

        uint8_t opcode = 0;
        int found_op = 1;

        if (strcmp(op, "PUSH") == 0)
            opcode = 0x01;
        else if (strcmp(op, "POP") == 0)
            opcode = 0x02;
        else if (strcmp(op, "DUP") == 0)
            opcode = 0x03;
        else if (strcmp(op, "ADD") == 0)
            opcode = 0x10;
        else if (strcmp(op, "SUB") == 0)
            opcode = 0x11;
        else if (strcmp(op, "MUL") == 0)
            opcode = 0x12;
        else if (strcmp(op, "DIV") == 0)
            opcode = 0x13;
        else if (strcmp(op, "CMP") == 0)
            opcode = 0x14;
        else if (strcmp(op, "JMP") == 0)
            opcode = 0x20;
        else if (strcmp(op, "JZ") == 0)
            opcode = 0x21;
        else if (strcmp(op, "JNZ") == 0)
            opcode = 0x22;
        else if (strcmp(op, "STORE") == 0)
            opcode = 0x30;
        else if (strcmp(op, "LOAD") == 0)
            opcode = 0x31;
        else if (strcmp(op, "CALL") == 0)
            opcode = 0x40;
        else if (strcmp(op, "RET") == 0)
            opcode = 0x41;
        else if (strcmp(op, "PRINT") == 0)
            opcode = 0x50;
        else if (strcmp(op, "NEW_PAIR") == 0)
            opcode = 0x60;
        else if (strcmp(op, "GC") == 0)
            opcode = 0x61;
        else if (strcmp(op, "PUSH_NIL") == 0)
            opcode = 0x62;
        else if (strcmp(op, "NEW_FUNCTION") == 0)
            opcode = 0x70;
        else if (strcmp(op, "NEW_CLOSURE") == 0)
            opcode = 0x71;
        else if (strcmp(op, "HALT") == 0)
            opcode = 0xFF;
        else
            found_op = 0;

        if (!found_op)
        {
            fprintf(stderr, "Error line %u: Unknown instruction '%s'\n", line_num, op);
            return 1;
        }

        fwrite(&opcode, 1, 1, out);

        if (needs_arg(op))
        {
            if (sscanf(line, "%*s %s", arg_s) != 1)
            {
                fprintf(stderr, "Error line %u: Instruction '%s' requires an argument\n", line_num, op);
                return 1;
            }

            int arg = 0; // Argument storage

            if (strcmp(op, "PUSH") == 0 || strcmp(op, "STORE") == 0 || strcmp(op, "LOAD") == 0)
            {
                if (sscanf(arg_s, "%d", &arg) != 1)
                {
                    fprintf(stderr, "Error line %u: Argument for '%s' must be an integer\n", line_num, op);
                    return 1;
                }
                fwrite(&arg, sizeof(int), 1, out); // Write the argument to output
            }
            else if (strcmp(op, "JMP") == 0 || strcmp(op, "JZ") == 0 || strcmp(op, "JNZ") == 0)
            {
                if (sscanf(arg_s, "%d", &arg) != 1)
                {
                    fprintf(stderr, "Error line %u: Argument for '%s' must be a valid address\n", line_num, op);
                    return 1;
                }
                fwrite(&arg, sizeof(int), 1, out); // Write the address to output
            }

            int32_t val = 0;
            int is_label = 0;

            for (int i = 0; i < label_cnt; i++)
            {
                if (strcmp(labels[i].name, arg_s) == 0)
                {
                    val = labels[i].addr;
                    is_label = 1;
                    break;
                }
            }

            if (!is_label)
            {
                char *ptr;
                val = strtol(arg_s, &ptr, 10);
                if (*ptr != '\0')
                {
                    fprintf(stderr, "Error line %u: Unrecognized label or invalid number '%s'\n", line_num, arg_s);
                    return 1;
                }
            }

            write_be32(val, out);
        }
    }

    fclose(in);
    fclose(out);
    printf("Successfully assembled to program.bin\n");
    return 0;
}
