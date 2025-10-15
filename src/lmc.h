// floason (C) 2025
// Licensed under the MIT License.

#pragma once

#include <stdio.h>
#include <stdbool.h>

#define NUM_MAILBOXES   100

// This struct is used for the mailboxes for LMC interpretation.
struct mailboxes
{
    union
    {
        short pool[NUM_MAILBOXES];
        char error_msg[NUM_MAILBOXES * 2];
    };
};

// Assemble an LMC program.
bool lmc_assemble(const char* buffer, size_t length, struct mailboxes* mailboxes);

// Execute an assembled LMC program.
void lmc_execute(struct mailboxes* mailboxes, FILE* outstream, FILE* errstream);