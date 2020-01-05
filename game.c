#include <assert.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "tetris.h"

#define BASE_SCORE_PER_ROW 10 /* score awarded for each row cleared */
#define MAX_ROWS_PER_LEVEL 10 /* rows to clear before next level */
#define INITIAL_TIMEOUT 1000

/* rule for calculating timeout reduction delta with each new level */
#define TIMEOUT_DELTA(level) ((DIFFICULTY_LEVEL_MAX - (level) + 1) * 3)

typedef enum {
    ACTION_DROP, /* drop the block at the floor */
    ACTION_MOVE_LEFT,
    ACTION_MOVE_RIGHT,
    ACTION_MOVE_DOWN,
    ACTION_ROTATE_LEFT,
    ACTION_PLACE_NEW,
    TOTAL_MOVEMENTS
} action_t;

struct thread_data {
    struct block *current;
    bool game_over;
    pthread_mutex_t lock;
    pthread_t thread_id;
};

int board[GAME_BOARD_HEIGHT + 2][GAME_BOARD_WIDTH + 2];

static block_t next_block = -1;
static degree_t next_block_orientation = -1;
static struct block current_block;

static const struct point starting_position = {4, 0};
const struct position positions[TOTAL_BLOCKS][TOTAL_DEGREES] =
    {
        [BLOCK_SQUARE] =
            {
                {{{1, 1}, {2, 1}, {1, 2}, {2, 2}}}, /* DEG_0 */
                {{{1, 1}, {2, 1}, {1, 2}, {2, 2}}}, /* DEG_90 */
                {{{1, 1}, {2, 1}, {1, 2}, {2, 2}}}, /* DEG_180 */
                {{{1, 1}, {2, 1}, {1, 2}, {2, 2}}}, /* DEG_270 */
            },
        [BLOCK_LINE] =
            {
                {{{0, 1}, {1, 1}, {2, 1}, {3, 1}}}, /* DEG_0 */
                {{{2, 0}, {2, 1}, {2, 2}, {2, 3}}}, /* DEG_90 */
                {{{0, 1}, {1, 1}, {2, 1}, {3, 1}}}, /* DEG_180 */
                {{{2, 0}, {2, 1}, {2, 2}, {2, 3}}}, /* DEG_270 */
            },
        [BLOCK_TEE] =
            {
                {{{2, 0}, {1, 1}, {2, 1}, {3, 1}}}, /* DEG_0 */
                {{{2, 0}, {2, 1}, {2, 2}, {3, 1}}}, /* DEG_90 */
                {{{2, 2}, {1, 1}, {2, 1}, {3, 1}}}, /* DEG_180 */
                {{{2, 0}, {2, 1}, {2, 2}, {1, 1}}}, /* DEG_270 */
            },
        [BLOCK_ZEE_1] =
            {
                {{{2, 0}, {1, 1}, {2, 1}, {1, 2}}}, /* DEG_0 */
                {{{1, 1}, {2, 1}, {2, 2}, {3, 2}}}, /* DEG_90 */
                {{{2, 0}, {1, 1}, {2, 1}, {1, 2}}}, /* DEG_180 */
                {{{1, 1}, {2, 1}, {2, 2}, {3, 2}}}, /* DEG_270 */
            },
        [BLOCK_ZEE_2] =
            {
                {{{1, 0}, {1, 1}, {2, 1}, {2, 2}}}, /* DEG_0 */
                {{{1, 1}, {2, 1}, {0, 2}, {1, 2}}}, /* DEG_90 */
                {{{1, 0}, {1, 1}, {2, 1}, {2, 2}}}, /* DEG_180 */
                {{{1, 1}, {2, 1}, {0, 2}, {1, 2}}}, /* DEG_270 */
            },
        [BLOCK_ELL_1] =
            {
                {{{1, 0}, {1, 1}, {1, 2}, {2, 2}}}, /* DEG_0 */
                {{{1, 1}, {2, 1}, {3, 1}, {1, 2}}}, /* DEG_90 */
                {{{1, 0}, {2, 0}, {2, 1}, {2, 2}}}, /* DEG_180 */
                {{{2, 0}, {0, 1}, {1, 1}, {2, 1}}}, /* DEG_270 */
            },
        [BLOCK_ELL_2] =
            {
                {{{2, 0}, {2, 1}, {2, 2}, {1, 2}}}, /* DEG_0 */
                {{{1, 0}, {1, 1}, {2, 1}, {3, 1}}}, /* DEG_90 */
                {{{1, 0}, {2, 0}, {1, 1}, {1, 2}}}, /* DEG_180 */
                {{{0, 1}, {1, 1}, {2, 1}, {2, 2}}}, /* DEG_270 */
            },
};

static void reset_game_board(void)
{
    memset(board, 0, sizeof(board));
    memset(board[0], ~0, sizeof(*board));
    memset(board[GAME_BOARD_HEIGHT + 1], ~0, sizeof(*board));
    for (int i = 0; i < GAME_BOARD_HEIGHT + 2; i++)
        board[i][0] = board[i][GAME_BOARD_WIDTH + 1] = ~0;
}

static void init_game_internals(void)
{
    /* initialize the board */
    reset_game_board();

    /* initialize the current and next block */
    srand((unsigned) time(NULL));
    next_block = (block_t)(rand() % TOTAL_BLOCKS);
    next_block_orientation = (degree_t)(rand() % TOTAL_DEGREES);
}

static struct block *update_current_block(struct block *block)
{
    if (!block)
        block = &current_block;

    block->type = next_block;
    block->orientation = next_block_orientation;

    next_block = (block_t)(rand() % TOTAL_BLOCKS);
    next_block_orientation = (degree_t)(rand() % TOTAL_DEGREES);

    return block;
}

static bool test_movement(struct block *block)
{
    bool status = true;
    for (int i = 0; i < ARRAY_SIZE(block->position->pos); i++) {
        int x = block->origin.x + block->position->pos[i].x;
        int y = block->origin.y + block->position->pos[i].y;
        if (board[y + 1][x + 1]) {
            status = false;
            break;
        }
    }
    return status;
}

static bool move_block(struct block *block, action_t movement)
{
    struct block newblock = *block; /* start with a copy of the given block */

    /* apply the requested operation */
    switch (movement) {
    case ACTION_MOVE_LEFT:
        --newblock.origin.x;
        assert(newblock.origin.x < GAME_BOARD_WIDTH);
        break;
    case ACTION_MOVE_RIGHT:
        ++newblock.origin.x;
        assert(newblock.origin.x < GAME_BOARD_WIDTH);
        break;
    case ACTION_MOVE_DOWN:
        ++newblock.origin.y;
        assert(newblock.origin.y < GAME_BOARD_HEIGHT);
        break;
    case ACTION_DROP:
        do {
            newblock.origin.y++;
        } while (test_movement(&newblock));

        newblock.origin.y--;
        assert(newblock.origin.y < GAME_BOARD_HEIGHT);
        break;
    case ACTION_ROTATE_LEFT:
        if (newblock.orientation == DEG_0)
            newblock.orientation = TOTAL_DEGREES - 1;
        else
            --newblock.orientation;
        newblock.position = &positions[newblock.type][newblock.orientation];
        break;
    case ACTION_PLACE_NEW:
        newblock.origin = starting_position;
        newblock.position = &positions[newblock.type][newblock.orientation];
        break; /* no change in position requested */
    default:
        return false;
    }

    /* check if the new changes can be applied */
    bool result = test_movement(&newblock);
    if (result)
        *block = newblock; /* apply the new change */
    return result;
}

static void main_loop(struct thread_data *data)
{
    while (1) {
        input_t input = get_user_input();
        if (input == INPUT_INVALID)
            continue;

        pthread_mutex_lock(&data->lock);

        /* check if the game is valid */
        if (data->game_over) {
            pthread_mutex_unlock(&data->lock);
            break;
        }

        /* in case there is no "current block", do nothing */
        if (input != INPUT_PAUSE_QUIT && !data->current) {
            pthread_mutex_unlock(&data->lock);
            continue;
        }

        switch (input) {
            bool status;
        case INPUT_MOVE_LEFT:
            if (move_block(data->current, ACTION_MOVE_LEFT))
                draw_game_board(data->current);
            break;
        case INPUT_MOVE_RIGHT:
            if (move_block(data->current, ACTION_MOVE_RIGHT))
                draw_game_board(data->current);
            break;
        case INPUT_DROP:
            if (move_block(data->current, ACTION_DROP))
                draw_game_board(data->current);
            break;
        case INPUT_ROTATE_LEFT:
            if (move_block(data->current, ACTION_ROTATE_LEFT))
                draw_game_board(data->current);
            break;
        case INPUT_PAUSE_QUIT:
            status = show_quit_dialog();
            draw_game_board(data->current);
            if (!status) /* if the user chooses to quit */
                data->game_over = true;
            break;
        case INPUT_TIMEOUT: /* nothing to do */
        default:
            break;
        }

        pthread_mutex_unlock(&data->lock);
    }
}

/* the nearest empty row (counting from bottom) in the board */
static int top_row = GAME_BOARD_HEIGHT;

static void freeze_block(struct block *current)
{
    static const int empty_row[GAME_BOARD_WIDTH + 2] = {
        ~0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, /* GAME_BOARD_WIDTH times */
        ~0,
    };

    /* fuse the current block with the board */
    for (int i = 0; i < ARRAY_SIZE(current->position->pos); i++) {
        int x = current->origin.x + current->position->pos[i].x;
        int y = current->origin.y + current->position->pos[i].y;
        assert(board[y + 1][x + 1] == 0);
        board[y + 1][x + 1] = 1; /* FIXME: change to supported colors */
    }

    /* now recompute the top_row */
    for (int i = GAME_BOARD_HEIGHT; i > 0; i--) {
        if (!memcmp(board[i], empty_row, sizeof(*board))) {
            top_row = i;
            break;
        }
    }
    assert(top_row > 0);
}

extern void draw_cleared_rows_animation(int *rows, int count);
static int clear_even_rows(void)
{
    int count = 0;
    int cleared_rows[4]; /* XXX: max 4 rows can be cleared at a time? */

    /* set animation style */
    static void (*clear_animation)(int *, int) = draw_cleared_rows_animation;

    for (int abs_row = GAME_BOARD_HEIGHT, i = GAME_BOARD_HEIGHT; i > top_row;
         abs_row--) {
        bool found = true;
        for (int j = 1; j < GAME_BOARD_WIDTH + 1; j++) {
            if (!board[i][j]) {
                found = false;
                break;
            }
        }

        if (found)
            cleared_rows[count++] = abs_row - 1; /* save the absolute row */
        else {
            i--;
            continue; /* move on to check the next row */
        }

        memset(board[i], 0, sizeof(*board)); /* clear the current row */
        /* move down the all the rows above the cleared row */
        memmove(board[top_row + 1], board[top_row],
                (i - top_row) * sizeof(*board));
        top_row++;
        assert(top_row <= GAME_BOARD_HEIGHT);
    }

    /* now animate (blink) the cleared rows */
    if (count)
        clear_animation(cleared_rows, count);
    return count;
}

static bool update_score_level(struct game_score *score,
                               int num_rows,
                               int *timeout)
{
    bool has_level_changed = false;
    score->score += score->level * num_rows * num_rows * BASE_SCORE_PER_ROW;

    score->total_rows += num_rows;
    score->rows_cleared += num_rows;

    if (score->rows_cleared >= MAX_ROWS_PER_LEVEL) {
        if (score->level < DIFFICULTY_LEVEL_MAX)
            *timeout -= TIMEOUT_DELTA(score->level);

        score->level++;
        score->rows_cleared = 0;
        has_level_changed = true;
    }

    return has_level_changed;
}

static void *worker_thread(void *arg)
{
    int timeout = INITIAL_TIMEOUT;
    struct game_score game_score = {
        .level = 1, .rows_cleared = 0, .total_rows = 0, .score = 0};
    struct thread_data *data = (struct thread_data *) arg;

    /* FIXME: setup initial timeout by reducing all delta upto initial_level */
    for (int i = 1; i <= /* initial_level */ 1; i++)
        timeout -= TIMEOUT_DELTA(i);

    draw_score_board(&game_score);

    /* main game loop */
    while (1) {
        /* wait till timeout */
        snooze(timeout);

        /* acquire the lock */
        pthread_mutex_lock(&data->lock);

        if (data->game_over) {
            pthread_mutex_unlock(&data->lock);
            break;
        }

        if (!data->current) {
            data->current = update_current_block(NULL);

            if (!move_block(data->current, ACTION_PLACE_NEW)) {
                data->game_over = true;
                pthread_mutex_unlock(&data->lock);
                break;
            }

            draw_next_block(next_block, next_block_orientation);
        } else {
            /* try to move the block downwards */
            if (!move_block(data->current, ACTION_MOVE_DOWN)) {
                /* freeze this block in the game board */
                freeze_block(data->current);
                data->current = NULL; /* reset the current block pointer */

                int num_rows = clear_even_rows();
                if (num_rows) {
                    int ret =
                        update_score_level(&game_score, num_rows, &timeout);
                    draw_score_board(&game_score);

                    /* see if the level has changed */
                    if (ret) {
                        draw_game_board(data->current);
                        draw_level_info(game_score.level);
                    }
                }
            }
        }

        draw_game_board(data->current);
        pthread_mutex_unlock(&data->lock);
    }

    return arg;
}

bool start_new_game(void)
{
    struct thread_data data = {.current = NULL,
                               .game_over = false,
                               .lock = PTHREAD_MUTEX_INITIALIZER,
                               .thread_id = 0};
    init_game_internals();
    init_game_screen();

    /* draw the next block and level info */
    draw_next_block(next_block, next_block_orientation);
    draw_level_info(/* initial_level */ 1);

    /* create the worker thread */
    if (pthread_create(&data.thread_id, NULL, worker_thread, (void *) &data))
        return false;

    main_loop(&data);
    pthread_join(data.thread_id, NULL);

    return true;
}
