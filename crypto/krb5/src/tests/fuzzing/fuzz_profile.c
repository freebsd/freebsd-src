/* -*- mode: c; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/* tests/fuzzing/fuzz_profile.c */
/*
 * Copyright (C) 2024 by Arjun. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Fuzzing harness implementation for profile_parse_file.
 */

#include "autoconf.h"
#include <prof_int.h>

void dump_profile(struct profile_node *root, int level);

#define kMinInputLength 2
#define kMaxInputLength 1024

extern int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size);

int
LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    errcode_t ret;
    FILE *fp_w, *fp_r;
    char file_name[256], *output;
    struct profile_node *root;

    if (size < kMinInputLength || size > kMaxInputLength)
        return 0;

    snprintf(file_name, sizeof(file_name), "/tmp/libfuzzer.%d", getpid());

    /* Write data into the file. */
    fp_w = fopen(file_name, "w");
    if (!fp_w)
        return 1;
    fwrite(data, 1, size, fp_w);
    fclose(fp_w);

    /* Provide the file pointer to the parser. */
    fp_r = fopen(file_name, "r");
    if (!fp_r)
        return 1;

    initialize_prof_error_table();

    ret = profile_parse_file(fp_r, &root, NULL);
    if (!ret) {
        ret = profile_write_tree_to_buffer(root, &output);
        if (!ret)
            free(output);

        profile_verify_node(root);
        profile_free_node(root);
    }

    fclose(fp_r);
    unlink(file_name);

    return 0;
}
