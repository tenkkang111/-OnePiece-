#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <signal.h>

#define WIDTH 40
#define HEIGHT 20
#define MAX_LEN 300

typedef struct {
    int x, y;
} Position;

typedef enum { UP, DOWN, LEFT, RIGHT } Direction;

Position snake[MAX_LEN];
int snake_length = 5;
Direction dir = RIGHT;
Position food;

int game_over = 0;
int score = 0;
int paused = 0;
volatile sig_atomic_t sigint_requested = 0;
volatile sig_atomic_t sigtstp_requested = 0;
int terminated_by_user = 0;

static int center_x(const char *text) {
    int interior_width = WIDTH - 1;
    int len = (int)strlen(text);
    return 1 + (interior_width - len) / 2;
}

static int position_on_snake(int x, int y) {
    for (int i = 0; i < snake_length; i++) {
        if (snake[i].x == x && snake[i].y == y) {
            return 1;
        }
    }
    return 0;
}

static void place_food(void) {
    do {
        food.x = rand() % (WIDTH - 2) + 1;
        food.y = rand() % (HEIGHT - 2) + 1;
    } while (position_on_snake(food.x, food.y));
}

static void prompt_exit(void) {
    timeout(-1);
    curs_set(TRUE);
    echo();

    const char *prompt = "Are you sure you want to quit? (y/n): ";
    int x = center_x(prompt);
    int y = HEIGHT / 2;

    mvhline(y, 1, ' ', WIDTH - 1);
    mvprintw(y, x, "%s", prompt);
    refresh();

    int ch;
    do {
        ch = getch();
    } while (ch != 'y' && ch != 'Y' && ch != 'n' && ch != 'N');

    if (ch == 'y' || ch == 'Y') {
        game_over = 1;
        terminated_by_user = 1;
    }

    mvhline(y, 1, ' ', WIDTH - 1);
    sigint_requested = 0;
    noecho();
    curs_set(FALSE);
    timeout(100);
}

void handle_signal(int sig) {
    if (sig == SIGINT) {
        sigint_requested = 1;
    } else if (sig == SIGTSTP) {
        sigtstp_requested = 1;
    }
}

/**
 * Initialize the game state and configure terminal settings.
 */
void init_game() {
    initscr();
    noecho();
    curs_set(FALSE);
    timeout(100);
    srand(time(NULL));

    for (int i = 0; i < snake_length; i++) {
        snake[i].x = WIDTH / 2 - i;
        snake[i].y = HEIGHT / 2;
    }

    // Generate the initial position of the food
    place_food();

    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = handle_signal;
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTSTP, &sa, NULL);
}

/**
 * End the game and clean up ncurses mode.
 */
void end_game() {
    endwin();
    if (terminated_by_user) {
        printf("Terminated by user. Final Score: %d\n", score);
    } else {
        printf("Game Over! Final Score: %d\n", score);
    }
}

/**
 * Draw the border around the game area using '#'.
 */
void draw_border() {
    for (int x = 0; x <= WIDTH; x++) {
        mvaddch(0, x, '#');
        mvaddch(HEIGHT, x, '#');
    }

    for (int y = 0; y <= HEIGHT; y++) {
        mvaddch(y, 0, '#');
        mvaddch(y, WIDTH, '#');
    }
}

/**
 * Draw the snake on the screen using 'O'.
 */
void draw_snake() {
    for (int i = 0; i < snake_length; i++) {
        mvprintw(snake[i].y, snake[i].x, "O");
    }
}

/**
 * Draw the food on the screen using '@'.
 */
void draw_food() {
    mvprintw(food.y, food.x, "@");
}

/**
 * Display the game status.
 */
void draw_status() {
    mvprintw(HEIGHT + 1, 0, "Score: %d, Length: %d", score, snake_length);
    clrtoeol();

    if (paused) {
        const char *pause_text = "== PAUSED ==";
        const char *resume_text = "Press 'p' or Ctrl+Z to resume";
        int pause_x = center_x(pause_text);
        int resume_x = center_x(resume_text);
        int pause_y = HEIGHT / 2;

        mvhline(pause_y, 1, ' ', WIDTH - 1);
        mvhline(pause_y + 1, 1, ' ', WIDTH - 1);
        mvprintw(pause_y, pause_x, "%s", pause_text);
        mvprintw(pause_y + 1, resume_x, "%s", resume_text);
    }
}

void move_snake() {
    // Move each segment to the position of the previous one
    for (int i = snake_length - 1; i > 0; i--) {
        snake[i] = snake[i - 1];
    }

    // Update the head's position based on the current direction
    switch (dir) {
        case UP:
            snake[0].y--;
            break;
        case DOWN:
            snake[0].y++;
            break;
        case LEFT:
            snake[0].x--;
            break;
        case RIGHT:
            snake[0].x++;
            break;
    }

    // Check for wall collision
    if (snake[0].x <= 0 || snake[0].x >= WIDTH ||
        snake[0].y <= 0 || snake[0].y >= HEIGHT) {
        game_over = 1;
        return;
    }

    // Check for collision with itself
    for (int i = 1; i < snake_length; i++) {
        if (snake[0].x == snake[i].x && snake[0].y == snake[i].y) {
            game_over = 1;
            return;
        }
    }

    // Check if food is eaten
    if (snake[0].x == food.x && snake[0].y == food.y) {
        if (snake_length < MAX_LEN) {
            snake_length++;
        }
        score++;
        place_food();
    }
}

void handle_input() {
    int ch = getch();

    switch (ch) {
        case 'w':
            if (dir != DOWN) {
                dir = UP;
            }
            break;
        case 's':
            if (dir != UP) {
                dir = DOWN;
            }
            break;
        case 'a':
            if (dir != RIGHT) {
                dir = LEFT;
            }
            break;
        case 'd':
            if (dir != LEFT) {
                dir = RIGHT;
            }
            break;
        case 'p':
            paused = !paused;
            break;
        case 'q':
            game_over = 1;
            break;
    }
}

int main() {
    init_game();

    while (!game_over) {
        clear();
        draw_border();
        draw_food();
        draw_snake();
        draw_status();

        handle_input();

        if (sigint_requested) {
            prompt_exit();
        }

        if (sigtstp_requested) {
            paused = !paused;
            sigtstp_requested = 0;
        }

        if (!paused && !game_over) {
            move_snake();
        }

        refresh();
        usleep(100000);
    }

    end_game();
    return 0;
}
