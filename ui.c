#include <ncurses.h>
#include <stdbool.h>

#include "tetris.h"

#define PRINT_BLOCK(window, pos_y, pos_x)                         \
    do {                                                          \
        mvwaddch((window), (pos_y), ((pos_x) << 1), ACS_CKBOARD); \
        waddch((window), ACS_CKBOARD);                            \
    } while (0)

#define GAME_INPUT_TIMEOUT 1000 /* getch() timeout */

static WINDOW *win_main, *win_game, *win_quit, *win_next, *win_score;

int snooze(int ms)
{
    return napms(ms);
}

/* check if the current window size is big enough */
static bool validate_screensize(int x, int y)
{
    if (x < WINDOW_MAIN_SIZE_X || y < WINDOW_MAIN_SIZE_Y) {
        endwin();
        fprintf(stderr,
                "Terminal size [%2d x %2d] is not sufficient to render.\n"
                "The minimum required size is [%2d x %2d]\n",
                x, y, WINDOW_MAIN_SIZE_X, WINDOW_MAIN_SIZE_Y);
        return false;
    }
    return true;
}

bool init_ui(void)
{
    int current_x, current_y;
    initscr();
    getmaxyx(stdscr, current_y, current_x);

    /* check if the current window size is big enough */
    if (!validate_screensize(current_x, current_y))
        return false;

    cbreak();
    noecho();
    curs_set(0);

    int start_x = ((current_x - WINDOW_MAIN_SIZE_X) / 2);
    int start_y = ((current_y - WINDOW_MAIN_SIZE_Y) / 2);

    /* FIXME: in case the screen size is larger than the required size,
     * the game should be placed at the center of the window.
     * FIXME: handle SIGWINCH for window change.
     */
    win_main = newwin(WINDOW_MAIN_SIZE_Y, WINDOW_MAIN_SIZE_X, start_y, start_x);
    win_game = newwin(20, 24, start_y + 2, start_x + 14);
    win_quit = newwin(7, 24, start_y + 7, start_x + 14);
    win_next = newwin(4, 8, start_y + 5, start_x + 48);
    win_score = newwin(7, 8, start_y + 14, start_x + 63);

    if (!win_main || !win_game || !win_quit || !win_next || !win_score) {
        fprintf(stderr, "Fail to initialize windows\n");
        return false;
    }

    keypad(stdscr, TRUE);
    keypad(win_game, TRUE);
    keypad(win_quit, TRUE);
    return true;
}

void deinit_ui(void)
{
    if (win_score)
        delwin(win_score);
    if (win_next)
        delwin(win_next);
    if (win_quit)
        delwin(win_quit);
    if (win_game)
        delwin(win_game);
    if (win_main)
        delwin(win_main);
    erase();
    refresh();
    endwin();
}

void init_game_screen(void)
{
    werase(win_main);

    mvwaddstr(win_main, 3, 47, "Next Block");
    mvwaddstr(win_main, 14, 47, "Level        :  ");
    mvwaddstr(win_main, 16, 47, "rows cleared :  ");
    mvwaddstr(win_main, 17, 47, "total rows   :  ");
    mvwaddstr(win_main, 18, 47, "Score        :  ");

    /* draw the next block window border */
    mvwaddch(win_main, 4, 46, ACS_ULCORNER);
    mvwaddch(win_main, 4, 57, ACS_URCORNER);
    mvwaddch(win_main, 9, 46, ACS_LLCORNER);
    mvwaddch(win_main, 9, 57, ACS_LRCORNER);

    mvwhline(win_main, 4, 47, ACS_HLINE, 10);
    mvwhline(win_main, 9, 47, ACS_HLINE, 10);
    mvwvline(win_main, 5, 46, ACS_VLINE, 4);
    mvwvline(win_main, 5, 57, ACS_VLINE, 4);

    /* draw the game window border */
    mvwaddch(win_main, 1, 13, ACS_ULCORNER);
    mvwaddch(win_main, 1, 38, ACS_URCORNER);
    mvwaddch(win_main, 22, 13, ACS_LLCORNER);
    mvwaddch(win_main, 22, 38, ACS_LRCORNER);

    mvwhline(win_main, 1, 14, ACS_HLINE, 24);
    mvwhline(win_main, 22, 14, ACS_HLINE, 24);
    mvwvline(win_main, 2, 13, ACS_VLINE, 20);
    mvwvline(win_main, 2, 38, ACS_VLINE, 20);

    wrefresh(win_main);
}

input_t get_user_input(void)
{
    input_t result;
    switch (wgetch(win_game)) {
    case ERR: /* timeout */
        result = INPUT_TIMEOUT;
        break;
    case KEY_LEFT:
        result = INPUT_MOVE_LEFT;
        break;
    case KEY_RIGHT:
        result = INPUT_MOVE_RIGHT;
        break;
    case KEY_DOWN:
    case ' ': /* space bar */
        result = INPUT_DROP;
        break;
    case KEY_UP:
        result = INPUT_ROTATE_LEFT;
        break;
    case 'Q':
    case 'q':
        result = INPUT_PAUSE_QUIT;
        break;
    default:
        result = INPUT_INVALID;
        break;
    }

    return result;
}

bool show_quit_dialog(void)
{
#define MESSAGE_QUIT()                           \
    do {                                         \
        mvwhline(win_quit, 3, 2, ACS_HLINE, 8);  \
        mvwhline(win_quit, 5, 2, ACS_HLINE, 8);  \
        mvwaddch(win_quit, 3, 1, ACS_ULCORNER);  \
        mvwaddch(win_quit, 4, 1, ACS_VLINE);     \
        mvwaddch(win_quit, 5, 1, ACS_LLCORNER);  \
        mvwaddch(win_quit, 3, 10, ACS_URCORNER); \
        mvwaddch(win_quit, 4, 10, ACS_VLINE);    \
        mvwaddch(win_quit, 5, 10, ACS_LRCORNER); \
        mvwprintw(win_quit, 4, 2, "  QUIT  ");   \
    } while (0)

#define MESSAGE_RESUME()                         \
    do {                                         \
        mvwhline(win_quit, 3, 14, ACS_HLINE, 8); \
        mvwhline(win_quit, 5, 14, ACS_HLINE, 8); \
        mvwaddch(win_quit, 3, 13, ACS_ULCORNER); \
        mvwaddch(win_quit, 4, 13, ACS_VLINE);    \
        mvwaddch(win_quit, 5, 13, ACS_LLCORNER); \
        mvwaddch(win_quit, 3, 22, ACS_URCORNER); \
        mvwaddch(win_quit, 4, 22, ACS_VLINE);    \
        mvwaddch(win_quit, 5, 22, ACS_LRCORNER); \
        mvwprintw(win_quit, 4, 14, " RESUME ");  \
    } while (0)

    const char *message =
        "                        "
        "       P A U S E D      "
        "                        "
        "                        "
        "                        "
        "                        "
        "                        ";
    wattron(win_quit, A_REVERSE | A_BOLD);
    mvwaddstr(win_quit, 0, 0, message);
    wattroff(win_quit, A_REVERSE | A_BOLD);

    enum { QUIT, RESUME };
    int choice = RESUME;
    bool need_refresh = true;
    /* the loop will accept only ENTER or SPACE */
    for (int input = 0; input != '\n' && input != ' ';) {
        if (need_refresh) {
            if (choice == RESUME) {
                MESSAGE_RESUME();
                wattron(win_quit, A_REVERSE | A_BOLD);
                MESSAGE_QUIT();
                wattroff(win_quit, A_REVERSE | A_BOLD);
            } else {
                MESSAGE_QUIT();
                wattron(win_quit, A_REVERSE | A_BOLD);
                MESSAGE_RESUME();
                wattroff(win_quit, A_REVERSE | A_BOLD);
            }
            wrefresh(win_quit);
        }

        need_refresh = true;
        input = wgetch(win_quit);
        switch (input) {
        case KEY_LEFT:
            choice = QUIT;
            break;
        case KEY_RIGHT:
            choice = RESUME;
            break;
        default:
            need_refresh = false;
            break;
        }
    }

    werase(win_quit);
    return (choice == RESUME);
#undef MESSAGE_RESUME
#undef MESSAGE_QUIT
}

void draw_next_block(block_t type, degree_t orientation)
{
    werase(win_next);
    for (int i = 0; i < ARRAY_SIZE(positions[type][orientation].pos); i++)
        PRINT_BLOCK(win_next, 0 + positions[type][orientation].pos[i].y,
                    0 + positions[type][orientation].pos[i].x);
    wrefresh(win_next);
}

void draw_game_board(struct block *block)
{
    werase(win_game);
    for (int i = 1; i < GAME_BOARD_HEIGHT + 1; i++) {
        for (int j = 1; j < GAME_BOARD_WIDTH + 1; j++) {
            if (board[i][j])
                PRINT_BLOCK(win_game, i - 1, j - 1);
        }
    }

    if (block) {
        for (int i = 0; i < ARRAY_SIZE(block->position->pos); i++)
            PRINT_BLOCK(win_game, block->origin.y + block->position->pos[i].y,
                        block->origin.x + block->position->pos[i].x);
    }
    wrefresh(win_game);
}

void draw_score_board(struct game_score *score)
{
    mvwprintw(win_score, 0, 0, "%-8d\n%-8d%-8d%-8d", score->level,
              score->rows_cleared, score->total_rows, score->score);
    wrefresh(win_score);
}

void draw_level_info(int level)
{
    const char *message =
        "                        "
        "     L E V E L   %02d     "
        "                        ";
    wattron(win_game, A_REVERSE | A_BOLD);
    mvwprintw(win_game, 7, 0, message, level);
    wattroff(win_game, A_REVERSE | A_BOLD);

    wrefresh(win_game);
    wtimeout(win_game, 1500); /* timeout of 1.5 secs for the getch() below */
    flushinp();
    wgetch(win_game);

    /* restore the timeout for gameplay */
    wtimeout(win_game, GAME_INPUT_TIMEOUT);
}

void draw_cleared_rows_animation(int *rows, int count)
{
#define width (GAME_BOARD_WIDTH << 1)
    static int direction = 0; /* animation from center or ends */

    snooze(300);
    if (++direction & 1) {
        for (int i = 0; i <= width / 2; i++) {
            for (int j = 0; j < count; j++) {
                mvwaddch(win_game, rows[j], i, ' ' | A_NORMAL);
                mvwaddch(win_game, rows[j], (width - i - 1), ' ' | A_NORMAL);
            }
            wrefresh(win_game);
            snooze(30);
        }
    } else {
        for (int i = width / 2; i >= 0; i--) {
            for (int j = 0; j < count; j++) {
                mvwaddch(win_game, rows[j], i, ' ' | A_NORMAL);
                mvwaddch(win_game, rows[j], (width - i - 1), ' ' | A_NORMAL);
            }
            wrefresh(win_game);
            snooze(30);
        }
    }
#undef width
}
