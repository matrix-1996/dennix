/* Copyright (c) 2016, 2017 Dennis Wölfing
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* libc/src/stdio/fgetc_unlocked.c
 * Gets a character from a file without locking.
 */

#include <unistd.h>
#include "FILE.h"

int fgetc_unlocked(FILE* file) {
    if (file->flags & FILE_FLAG_EOF) return EOF;

    unsigned char result;
    ssize_t bytesRead = read(file->fd, &result, 1);
    if (bytesRead == 0) {
        file->flags |= FILE_FLAG_EOF;
        return EOF;
    } else if (bytesRead < 0) {
        file->flags |= FILE_FLAG_ERROR;
        return EOF;
    }

    return result;
}
