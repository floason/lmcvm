// floason (C) 2025
// Licensed under the MIT License.

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

#include "lmc.h"
#include "util.h"

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        puts("usage: lmcvm path");
        return 0;
    }

    char* buffer;
    FILE* file;
    errno_t err = fopen_s(&file, argv[1], "r");
    if (err)
    {
        fprintf(stderr, "Could not read file \"%s\": %s\n", argv[1], strerror(err));
        return 1;
    }

    fseek(file, 0, SEEK_END);
    size_t length = ftell(file);
    buffer = (char*)quick_malloc(length);
    fseek(file, 0, SEEK_SET);
    fread(buffer, 1, length, file);
    fclose(file);

    struct mailboxes mailboxes;
    mailboxes.instream = stdin;
    mailboxes.outstream = stdout;

    bool result = lmc_assemble(buffer, length, &mailboxes);
    free(buffer);
    if (!result)
        goto fail;

    if (!lmc_execute(&mailboxes))
        goto fail;

    return 0;

fail:
    puts(mailboxes.error_msg);
    return 1;
}