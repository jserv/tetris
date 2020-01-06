#ifndef __TETRIS_H__
#define __TETRIS_H__

/* Copyright (c) 2020 National Cheng Kung University, Taiwan.
 * All rights reserved.
 * Use of this source code is governed by a MIT license that can be
 * found in the LICENSE file.
 */

#define WINDOW_MAIN_SIZE_X 80
#define WINDOW_MAIN_SIZE_Y 24

#define DIFFICULTY_LEVEL_MAX 25

#define GAME_BOARD_HEIGHT 20
#define GAME_BOARD_WIDTH 12

#define ARRAY_SIZE(arr) ((int) (sizeof(arr) / sizeof(*(arr))))

typedef enum {
    BLOCK_SQUARE,
    BLOCK_LINE,
    BLOCK_TEE,
    BLOCK_ZEE_1,
    BLOCK_ZEE_2,
    BLOCK_ELL_1,
    BLOCK_ELL_2,
    TOTAL_BLOCKS,
} block_t;

typedef enum { DEG_0, DEG_90, DEG_180, DEG_270, TOTAL_DEGREES } degree_t;

struct point {
    int x, y;
};

struct position {
    struct point pos[4];
};

struct block {
    block_t type;
    struct point origin;
    degree_t orientation;
    const struct position *position;
};

struct game_score {
    int level, rows_cleared, total_rows, score;
};

typedef enum {
    INPUT_TIMEOUT,
    INPUT_INVALID,
    INPUT_MOVE_LEFT,
    INPUT_MOVE_RIGHT,
    INPUT_DROP,
    INPUT_ROTATE_LEFT,
    INPUT_PAUSE_QUIT,
} input_t;

extern int board[GAME_BOARD_HEIGHT + 2][GAME_BOARD_WIDTH + 2];
extern const struct position positions[TOTAL_BLOCKS][TOTAL_DEGREES];

int snooze(int ms);
bool start_new_game(void);
input_t get_user_input(void);

bool init_ui(void);
void deinit_ui(void);
void init_game_screen(void);

bool show_quit_dialog(void);
void draw_next_block(block_t type, degree_t orientation);
void draw_game_board(struct block *current);
void draw_score_board(struct game_score *score);
void draw_level_info(int level);

#endif /* __TETRIS_H__ */
