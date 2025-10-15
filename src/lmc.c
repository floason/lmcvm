// floason (C) 2025
// Licensed under the MIT License.

#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "lmc.h"
#include "util.h"

#define OPCODE_LIST \
    X(HLT)          \
    X(ADD)          \
    X(SUB)          \
    X(STA)          \
    X(DAT)          \
    X(LDA)          \
    X(BRA)          \
    X(BRZ)          \
    X(BRP)          \
    X(INP)          \
    X(OUT)          \
    X(OP_COUNT)     \
    X(OP_NULL)

enum opcode
{
#define X(name) name,
    OPCODE_LIST
#undef X
};

static const char* op_names[] =
{
#define X(name) #name,
    OPCODE_LIST
#undef X
};

// Pascal string.
struct pstring
{
    const char* string;
    size_t length;
    size_t line;
    size_t column;
};

// The intermediate representation for an LMC opcode will consist of node structs which are
// linked together. Each instance will contain an optional label attribute, the opcode type
// (or the first digit for storing data with DAT - could be considered the segment), and an 
// address offset.
struct ir_node
{
    struct pstring label;
    enum opcode op;
    char offset;
    struct pstring label_offset;
    struct ir_node* next;
};

void cleanup_ir_node(struct ir_node* node)
{
    if (node->next)
        cleanup_ir_node(node->next);
    free(node);
}

int program_isspace(int c)
{
    return isspace(c) || c == '\0';
}

// Assemble an LMC program. This is a two-pass assembler (the first pass generates an 
// intermediate representation, whereas the second assembles the actual mailboxes).
bool lmc_assemble(const char* buffer, size_t length, struct mailboxes* mailboxes)
{
    memset(mailboxes->pool, 0, NUM_MAILBOXES);

    // Hold an array of all cached labels.
    struct pstring cached_labels[NUM_MAILBOXES] = { { NULL, 0 } };

    // This is a strange tokeniser, but I'm trying to minimize memory allocations here.
    // Effectively offset into tokens within the buffer and use strncmp for string
    // comparisons. This makes up the first pass of the assembler.
    struct ir_node* head = (struct ir_node*)quick_malloc(sizeof(struct ir_node));
    struct ir_node* ir = head;
    struct pstring start = { NULL };
    struct pstring erroneous_token;
    size_t offset = 0;
    size_t address = 0;
    size_t column = 1;
    ir->op = OP_NULL;
    ir->offset = -1;
    while (offset < length)
    {
        if (!program_isspace(buffer[offset]))
        {
            start.length++;
            if (start.string == NULL)
            {
                start.string = &buffer[offset];
                start.line = address + 1;
                start.column = column;
            }
        }

        if (program_isspace(buffer[offset]) || (offset + 1) >= length)
        {
            if (start.string != NULL)
            {
                // Decode the opcode used where applicable. A trie/hashmap would be more ideal
                // in this context, but since this is a small instruction set, I'm not too
                // concerned.
                enum opcode op = (isalpha(start.string[0]) && ir->op == OP_NULL && start.length == 3) 
                               ? HLT 
                               : OP_COUNT;
                for (; op < OP_COUNT; op++)
                {
                    if (util_strncasecmp(start.string, op_names[op], 3) == 0)
                    {
                        // INP and OUT are encoded as 901 and 902 respectively.
                        ir->op = min(op, 9);
                        if (ir->op == 9)
                            ir->offset = max(0, op - 8);
                        break;
                    }
                }

                // Was this actually an opcode that was decoded?
                if (op == OP_COUNT)
                {
                    // Check if a label was decoded instead.
                    if (ir->label.string == NULL && isalpha(start.string[0]) && ir->op == OP_NULL)
                    {
                        ir->label = start;
                        cached_labels[address] = start;
                    }

                    // This might've been an offset instead?
                    else if (ir->offset == -1 && ir->label_offset.string == NULL && ir->op != OP_NULL)
                    {
                        if (isdigit(start.string[0]))
                        {
                            int num = strtol(start.string, NULL, 10);
                            if (ir->op == DAT)
                                ir->op = (num / 100) % 10;
                            ir->offset = num % 100;
                        }
                        else
                            ir->label_offset = start;
                    }

                    // Something went wrong.
                    else
                    {
                        erroneous_token = start;
                        goto assembly_fail;
                    }
                }

                // If a newline is reached, then this is the end of the current instruction.
                if (buffer[offset] == '\n' || (offset + 1) >= length)
                {
                    if (ir->op == OP_NULL)
                    {
                        erroneous_token = ir->label;
                        goto assembly_fail;
                    }

                    ir->next = (struct ir_node*)quick_malloc(sizeof(struct ir_node));
                    ir = ir->next;
                    ir->op = OP_NULL;
                    ir->offset = -1;
                    address++;
                    column = 0; // Column is always incremented below.
                }
            }

            start.string = NULL;
            start.length = 0;
        }
        
        offset++;
        column++;
        continue;
    }
    
    // If there is an invalid opcode at the end, exit.
    if (ir->label.string != NULL && ir->op == OP_NULL)
    {
        erroneous_token = ir->label;
        goto assembly_fail;
    }

    // The second pass of the assembler is responsible for actually assembling the
    // mailboxes.
    ir = head;
    while (ir)
    {
        if (ir->op == OP_NULL)
            break;

        // this is testing code - DAT/OUT will be incorrectly reported due to internal hacks
        puts("instruction parsed:");
        printf("LABEL: %s\nINSTRUCTION: %s (%d | %s)\n\n", ir->label.string, op_names[ir->op], ir->offset, ir->label_offset.string);

        ir = ir->next;
    }

    cleanup_ir_node(head);
    return true;

assembly_fail:
    sprintf_s(mailboxes->error_msg, sizeof(mailboxes->error_msg), "Unknown token on line %d:%d: ",
              erroneous_token.line,
              erroneous_token.column);
    strncat_s(mailboxes->error_msg, 
              sizeof(mailboxes->error_msg), 
              erroneous_token.string,
              erroneous_token.length);
    cleanup_ir_node(head);
    return false;
}

// Execute an assembled LMC program.
void lmc_execute(struct mailboxes* mailboxes, FILE* outstream, FILE* errstream)
{

}