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
    struct ir_node* current;
    struct ir_node* next = node;
    while ((current = next) != NULL)
    {
        next = current->next;
        free(current);
    }
}

bool program_isspace(int c)
{
    return isspace(c) || c == '\0' || c == ';';
}

bool program_eol(int c)
{
    return c == '\n' || c == ';';
}

// Assemble an LMC program. This is a two-pass assembler (the first pass generates an 
// intermediate representation, whereas the second assembles the actual mailboxes).
bool lmc_assemble(const char* buffer, size_t length, struct mailboxes* mailboxes)
{
    memset(mailboxes->pool, 0, NUM_MAILBOXES);

    // Hold an array of all cached labels. A trie/dictionary would be more efficient,
    // but there's only 100 mailboxes and this is a basic assembler anyway.
    struct pstring cached_labels[NUM_MAILBOXES] = { { NULL, 0 } };

    // This is a strange tokeniser, but I'm trying to minimize memory allocations here.
    // Effectively offset into tokens within the buffer and use strncmp for string
    // comparisons. This makes up the first pass of the assembler.
    bool is_comment = false;
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
        if (is_comment)
        {
            if (buffer[offset++] == '\n')
                is_comment = false;
            continue;
        }

        // If the character is a semicolon, the rest of the line is a comment.
        if (buffer[offset] == ';')
        {
            is_comment = true;
            if (ir->label.string == NULL && ir->op == OP_NULL && start.string == NULL)
            {
                offset++;
                continue;
            }
        }

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
                        goto lexer_fail;
                    }
                }
            }

            // If EoL is reached, then this is the end of the current instruction.
            if (program_eol(buffer[offset]) || (offset + 1) >= length)
            {
                if (ir->op == OP_NULL)
                {
                    if (ir->label.string != NULL)
                    {                        
                        erroneous_token = ir->label;
                        goto lexer_fail;
                    }
                }
                else
                {
                    ir->next = (struct ir_node*)quick_malloc(sizeof(struct ir_node));
                    ir = ir->next;
                    ir->op = OP_NULL;
                    ir->offset = -1;
                    address++;
                    column = 0; // Column is always incremented below.

                    // The program must not be bigger than 100 mailboxes.
                    if (address >= NUM_MAILBOXES)
                    {
                        strcpy_s(mailboxes->error_msg, sizeof(mailboxes->error_msg), 
                                 "Program is too large");
                        goto compiler_fail;
                    }
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
        goto lexer_fail;
    }

    // The second pass of the assembler is responsible for actually assembling the
    // mailboxes.
    ir = head;
    address = 0;
    while (ir)
    {
        if (ir->op == OP_NULL)
            break;

        short value = ir->op * 100;
        if (ir->label_offset.string != NULL)
        {
            bool label_found = false;
            for (int i = 0; i < NUM_MAILBOXES; ++i)
            {
                if (ir->label_offset.length != cached_labels[i].length)
                    continue;
                if (util_strncasecmp(ir->label_offset.string, cached_labels[i].string, 
                    ir->label_offset.length) != 0)
                    continue;
                label_found = true;
                value += i;
            }

            if (!label_found)
            {
                erroneous_token = ir->label_offset;
                goto lexer_fail;
            }
        }
        else if (ir->offset != -1)
            value += ir->offset % 100;
        mailboxes->pool[address++] = value;

        ir = ir->next;
    }

    cleanup_ir_node(head);
    return true;

lexer_fail:
    sprintf_s(mailboxes->error_msg, sizeof(mailboxes->error_msg), "Unknown token on line %d:%d: ",
              erroneous_token.line, erroneous_token.column);
    strncat_s(mailboxes->error_msg, 
              sizeof(mailboxes->error_msg), 
              erroneous_token.string,
              erroneous_token.length);
compiler_fail:
    cleanup_ir_node(head);
    return false;
}

// Execute an assembled LMC program.
bool lmc_execute(struct mailboxes* mailboxes)
{
    // LMC registers.
    unsigned char pc = 0;       // Program counter.
    unsigned char ar = 0;       // Address register.
    short acc = 0;              // Accumulator.
    enum opcode ir = OP_NULL;   // Instruction register.
    
    // This LMC interpreter assumes that the accumulator can only hold 3-digit
    // numbers ranging from 0-999, therefore negative numbers cannot be
    // realistically handled. Some simulators treat this differently (i.e.
    // tolerate -999 to 999, or just straight up undefined behaviour). Instead,
    // this interpreter incorporates a negative flag that is set to true if
    // a subtraction calculation underflows.
    bool negative = false;

    for (;;)
    {
        // Fetch the opcode from the current mailbox.
        short data = mailboxes->pool[pc];
        pc = (pc + 1) % 100;

        // Decode the fetched opcode.
        ir = data / 100 + ((data / 100 == INP) ? data % 100 - 1 : 0);
        ar = data % 100;
        
        // Execute the fetched opcode.
        switch (ir)
        {
            case HLT:
                goto halt;
            case ADD:
            {
                negative = false;
                acc = (acc + mailboxes->pool[ar]) % 1000;
                break;
            }
            case SUB:
            {
                negative = (acc < mailboxes->pool[ar]);
                acc = (acc - mailboxes->pool[ar]) % 1000;
                break;
            }
            case STA:
            {
                mailboxes->pool[ar] = acc;
                break;
            }
            case LDA:
            {
                negative = false;
                acc = mailboxes->pool[ar];
                break;
            }
            case BRA:
            {
                pc = ar;
                break;
            }
            case BRZ:
            {
                if (acc == 0)
                    pc = ar;
                break;
            }
            case BRP:
            {
                if (!negative)
                    pc = ar;
                break;
            }
            case INP:
            {
                // A three-digit value is read from the input file stream into
                // the accumulator, setting the negative flag where necessary.
                // A char buffer of size 5 is selected to account for the negative,
                // sign maximum digit length and the terminating NUL character.
                char buffer[5];
                fgets(buffer, sizeof(buffer), mailboxes->instream);
                negative = (buffer[0] == '-');
                if (negative)
                    acc = atoi(&buffer[1]);
                else
                    acc = atoi(buffer);
                break;
            }
            case OUT:
            {
                // A newline-terminated string of the three-digit accumulator is
                // sent to to the output file stream.
                fprintf(mailboxes->outstream, "%d\n", acc);
                break;
            }
            default:
            {
                sprintf_s(mailboxes->error_msg, sizeof(mailboxes->error_msg), "Unknown opcode %d",
                          ir);
                return false;
            }
        }
    }

halt:
    return true;
}