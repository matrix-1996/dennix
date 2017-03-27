/* Copyright (c) 2017 Dennis Wölfing
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

/* editor/editor.c
 * Text editor.
 */

#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct line {
    char* buffer;
    size_t length;
    size_t bufferSize;
};

#define CTRL(c) ((c) & 0x1F)

static const size_t height = 25;
static const size_t width = 80;

static size_t cursorX;
static size_t cursorY;

static struct line* lines;
static size_t linesUsed;
static size_t linesAllocated;

static struct termios oldTermios;

static size_t windowX;
static size_t windowY;

static void drawLines(void);
static void getInput(void);
static void handleKey(unsigned char c);
static void handleSequence(unsigned char c);
static void readFile(const char* filename);
static void restoreTermios(void);
static void updateCursorPosition(void);

int main(int argc, char* argv[]) {
    const char* filename = NULL;
    if (argc >= 2) {
        filename = argv[1];
    }

    tcgetattr(0, &oldTermios);
    atexit(restoreTermios);
    struct termios newTermios = oldTermios;
#ifdef IXON
    newTermios.c_iflag &= ~IXON;
#endif
    newTermios.c_lflag &= ~(ECHO | ICANON);
    newTermios.c_cc[VMIN] = 0;
    tcsetattr(0, TCSAFLUSH, &newTermios);

#ifndef __dennix__
    setbuf(stdout, NULL);
#endif

    fputs("\e[2J", stdout);

    readFile(filename);
    drawLines();
    updateCursorPosition();
    while (true) {
        getInput();
    }
}

static void drawLines(void) {
    for (size_t y = 1; y <= height; y++) {
        printf("\e[%zuH\e[2K", y);
        if (y + windowY - 1 >= linesUsed) continue;

        struct line line = lines[y + windowY - 1];
        if (line.length <= windowX) continue;
        size_t length = line.length - windowX;
        if (length > width) {
            length = width;
        }
        fwrite(line.buffer + windowX, 1, length, stdout);
    }
}

static enum {
    NORMAL,
    ESCAPED,
    SEQUENCE
} state = NORMAL;

static void getInput(void) {
    unsigned char c;
    if (read(0, &c, 1) == 1) {
        if (state == NORMAL) {
            handleKey(c);
        } else if (state == ESCAPED) {
            if (c == '[') {
                state = SEQUENCE;
            } else {
                handleKey(c);
                state = NORMAL;
            }
        } else if (state == SEQUENCE) {
            handleSequence(c);
            state = NORMAL;
        }
    }
}

static void handleKey(unsigned char c) {
    switch (c) {
    case '\e':
        state = ESCAPED;
        break;
    case CTRL('Q'):
        fputs("\e[H\e[2J", stdout);
        exit(0);
    }
}

static void handleSequence(unsigned char c) {
    switch (c) {
    case 'A':
        if (cursorY > 0) {
            cursorY--;
        } else if (windowY > 0) {
            windowY--;
            drawLines();
        }
        break;
    case 'B':
        if (cursorY < height - 1 && windowY + cursorY < linesUsed) {
            cursorY++;
        } else if (windowY + cursorY < linesUsed) {
            windowY++;
            drawLines();
        }
        break;
    case 'C':
        if (cursorX < width - 1) {
            cursorX++;
        } else if (cursorX + windowX < lines[cursorY + windowY].length) {
            windowX++;
            drawLines();
        }
        break;
    case 'D':
        if (cursorX > 0) {
            cursorX--;
        } else if (windowX > 0) {
            windowX--;
            drawLines();
        }
        break;
    }

    if (windowX >= lines[cursorY + windowY].length) {
        windowX = lines[cursorY + windowY].length > 0 ?
                lines[cursorY + windowY].length - 1 : 0;
        cursorX = lines[cursorY + windowY].length - windowX;
        drawLines();
    } else if (cursorX + windowX > lines[cursorY + windowY].length) {
        cursorX = lines[cursorY + windowY].length - windowX;
    }
    updateCursorPosition();
}

static void readFile(const char* filename) {
    if (!filename) {
        if (linesAllocated == 0) {
            lines = malloc(sizeof(struct line));
            if (!lines) err(1, "malloc");
            linesAllocated = 1;
        }

        linesUsed = 0;
        lines[0].buffer = NULL;
        lines[0].bufferSize = 0;
        lines[0].length = 0;
        return;
    }

    FILE* file = fopen(filename, "r");
    if (!file) err(1, "'%s'", filename);

    while (true) {
        if (linesUsed == linesAllocated) {
            // TODO: Possible overflow
            size_t newSize = 2 * linesAllocated * sizeof(struct line);
            if (newSize == 0) {
                newSize = 32 * sizeof(struct line);
            }
            void* newBuffer = realloc(lines, newSize);
            if (!newBuffer) err(1, "realloc");
            lines = newBuffer;
            linesAllocated = newSize / sizeof(struct line);
        }

        struct line* currentLine = lines + linesUsed;

        currentLine->buffer = NULL;
        currentLine->bufferSize = 0;
        ssize_t length = getline(&currentLine->buffer,
                &currentLine->bufferSize, file);

        if (length == -1) {
            currentLine->length = 0;
            if (ferror(file)) err(1, "'%s'", filename);
            break;
        }

        if (currentLine->buffer[length - 1] != '\n') {
            if (currentLine->bufferSize < (size_t) length + 1) {
                void* newBuffer = realloc(currentLine->buffer, length + 1);
                if (!newBuffer) err(1, "realloc");
                currentLine->buffer = newBuffer;
                currentLine->bufferSize = length + 1;
            }
            currentLine->buffer[length++] = '\n';
            currentLine->buffer[length] = '\0';
        }

        currentLine->length = length - 1;
        linesUsed++;
    }

    fclose(file);
}

static void restoreTermios(void) {
    tcsetattr(0, TCSAFLUSH, &oldTermios);
}

static void updateCursorPosition(void) {
    printf("\e[%zu;%zuH", cursorY + 1, cursorX + 1);
}
