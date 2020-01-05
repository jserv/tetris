#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "tetris.h"

int main(void)
{
    /* register exit handler */
    if (atexit(deinit_ui)) {
        fprintf(stderr, "Fail to register exit handlers\n");
        return -1;
    }

    if (!init_ui()) {
        fprintf(stderr, "Fail to initialize UI\n");
        return -1;
    }

    if (!start_new_game())
        return -1;
    return 0;
}
