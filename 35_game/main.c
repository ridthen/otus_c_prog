#include <allegro5/allegro5.h>
#include <allegro5/allegro_primitives.h>
#include <allegro5/allegro_font.h>
#include <allegro5/allegro_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>


static ALLEGRO_DISPLAY *display = NULL;
static ALLEGRO_EVENT_QUEUE *queue = NULL;
static ALLEGRO_FONT *font = NULL;
static ALLEGRO_TIMER *timer = NULL;


static void cleanup(void) {
    if (timer) al_destroy_timer(timer);
    if (font) al_destroy_font(font);
    if (queue) al_destroy_event_queue(queue);
    if (display) al_destroy_display(display);
    al_shutdown_primitives_addon();
    al_shutdown_ttf_addon();
    al_shutdown_font_addon();
    al_uninstall_system();
}

/* Состояния игры */
typedef enum {
    MENU,
    PLAYING,
    QUIT
} GameState;


static char board[3][3];
static bool player_x_turn; /* true = ход X, false = ход O */
static GameState state = MENU;


/**
 * Проверка победы
 * @return 'X', 'O' или 0, если игра продолжается
 */
static char check_winner(void) {
    for (int i = 0; i < 3; ++i) {
        if (board[i][0] && board[i][0] == board[i][1]
            && board[i][1] == board[i][2])
            return board[i][0];
        if (board[0][i] && board[0][i] == board[1][i]
            && board[1][i] == board[2][i])
            return board[0][i];
    }
    if (board[0][0] && board[0][0] == board[1][1]
        && board[1][1] == board[2][2])
        return board[0][0];
    if (board[0][2] && board[0][2] == board[1][1]
        && board[1][1] == board[2][0])
        return board[0][2];
    return 0;
}


/**
 * Проверка заполненности поля
 * @return
 */
static bool is_board_full(void) {
    for (int i = 0; i < 3; ++i)
        for (int j = 0; j < 3; ++j)
            if (!board[i][j]) return false;
    return true;
}


/**
 * Отрисовка меню
 */
static void draw_menu(void) {
    al_clear_to_color(al_map_rgb(30, 30, 30));
    al_draw_text(font, al_map_rgb(255, 255, 255),
                 400, 200, ALLEGRO_ALIGN_CENTER,
                 "Крестики-нолики");

    /* Кнопка "Play" */
    al_draw_filled_rectangle(350, 300, 450, 350,
        al_map_rgb(70, 130, 180));
    al_draw_text(font, al_map_rgb(255, 255, 255),
                 400, 315, ALLEGRO_ALIGN_CENTER, "Играть");

    /* Кнопка "Exit" */
    al_draw_filled_rectangle(350, 400, 450, 450,
        al_map_rgb(180, 70, 70));
    al_draw_text(font, al_map_rgb(255, 255, 255),
                 400, 415, ALLEGRO_ALIGN_CENTER, "Выход");

    al_flip_display();
}

/**
 * Отрисовка игрового поля
 */
static void draw_board(void) {
    al_clear_to_color(al_map_rgb(240, 240, 240));

    /* Сетка */
    al_draw_line(266, 100, 266, 500,
        al_map_rgb(0, 0, 0), 4);
    al_draw_line(533, 100, 533, 500,
        al_map_rgb(0, 0, 0), 4);
    al_draw_line(100, 233, 700, 233,
        al_map_rgb(0, 0, 0), 4);
    al_draw_line(100, 366, 700, 366,
        al_map_rgb(0, 0, 0), 4);

    int cell_x[3] = {183, 400, 617};
    int cell_y[3] = {167, 300, 433};

    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            int x = cell_x[j];
            int y = cell_y[i];
            if (board[i][j] == 'X') {
                al_draw_line(x - 50, y - 50, x + 50, y + 50,
                             al_map_rgb(200, 50, 50), 8);
                al_draw_line(x + 50, y - 50, x - 50, y + 50,
                             al_map_rgb(200, 50, 50), 8);
            } else if (board[i][j] == 'O') {
                al_draw_circle(x, y, 50, al_map_rgb(50, 50,
                    200), 8);
            }
        }
    }

    /* Сообщение о победителе или ничьей */
    char winner = check_winner();
    if (winner) {
        char msg[32] = {0};
        snprintf(msg, sizeof(msg), "%c выиграл! Клик", winner);
        al_draw_text(font, al_map_rgb(0, 0, 0), 400, 550,
                     ALLEGRO_ALIGN_CENTER, msg);
    } else if (is_board_full()) {
        al_draw_text(font, al_map_rgb(0, 0, 0), 400, 550,
                     ALLEGRO_ALIGN_CENTER, "Ничья! Клик");
    } else {
        const char *turn_msg = player_x_turn ? "X ходит" : "O ходит";
        al_draw_text(font, al_map_rgb(0, 0, 0), 400, 550,
                     ALLEGRO_ALIGN_CENTER, turn_msg);
    }

    al_flip_display();
}

/**
 * Обработка клика по полю
 * @param mx
 * @param my
 */
static void handle_board_click(int mx, int my) {
    if (check_winner() || is_board_full()) {
        state = MENU;
        return;
    }

    int col = -1, row = -1;

    if (mx >= 100 && mx < 266) col = 0;
    else if (mx >= 266 && mx < 533) col = 1;
    else if (mx >= 533 && mx <= 700) col = 2;

    if (my >= 100 && my < 233) row = 0;
    else if (my >= 233 && my < 366) row = 1;
    else if (my >= 366 && my <= 500) row = 2;

    if (row != -1 && col != -1 && board[row][col] == 0) {
        board[row][col] = player_x_turn ? 'X' : 'O';
        player_x_turn = !player_x_turn;
    }
}

/**
 * Сброс игры к начальному состоянию
 */
static void reset_game(void) {
    memset(board, 0, sizeof(board));
    player_x_turn = true;
}

/**
 * Поиск TTF-шрифта
 * @param size
 * @return
 */
static ALLEGRO_FONT* load_cyrillic_font(int size) {
    ALLEGRO_FONT* loaded_font = NULL;
    const char* font_paths[] = {
        "data/Roboto-Regular.ttf",
        "Roboto-Regular.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "C:/Windows/Fonts/arial.ttf"
    };

    for (size_t i = 0; i < sizeof(font_paths) / sizeof(font_paths[0]); ++i) {
        loaded_font = al_load_ttf_font(font_paths[i], size, 0);
        if (loaded_font) {
            /* printf("Loaded font: %s\n", font_paths[i]); */
            return loaded_font;
        }
    }

    perror("al_load_ttf_font: could not load any Cyrillic font");
    return NULL;
}

int main(void) {
    if (!al_init()) {
        perror("al_init");
        goto error;
    }
    if (!al_install_mouse()) {
        perror("al_install_mouse");
        goto error;
    }
    if (!al_install_keyboard()) {
        perror("al_install_keyboard");
        goto error;
    }
    if (!al_init_primitives_addon()) {
        perror("al_init_primitives_addon");
        goto error;
    }
    al_init_font_addon();
    if (!al_init_ttf_addon()) {
        perror("al_init_ttf_addon");
        goto error;
    }

    if (atexit(cleanup) != 0) {
        perror("atexit");
        goto error;
    }

    display = al_create_display(800, 600);
    if (!display) {
        perror("al_create_display");
        goto error;
    }
    al_set_window_title(display, "Крестики-нолики");

    timer = al_create_timer(1.0 / 60.0);
    if (!timer) {
        perror("al_create_timer");
        goto error;
    }

    queue = al_create_event_queue();
    if (!queue) {
        perror("al_create_event_queue");
        goto error;
    }
    al_register_event_source(queue, al_get_mouse_event_source());
    al_register_event_source(queue, al_get_keyboard_event_source());
    al_register_event_source(queue, al_get_display_event_source(display));
    al_register_event_source(queue, al_get_timer_event_source(timer));

    // font = al_create_builtin_font();
    // if (!font) {
    //     perror("al_create_builtin_font");
    //     exit(EXIT_FAILURE);
    // }

    font = load_cyrillic_font(24);
    if (!font) {
        goto error;
    }

    reset_game();
    state = MENU;

    /* Главный цикл */
    al_start_timer(timer);
    bool redraw = true;

    while (state != QUIT) {
        ALLEGRO_EVENT event;
        al_wait_for_event(queue, &event);

        switch (event.type) {
            case ALLEGRO_EVENT_TIMER:
                redraw = true;
                break;

            case ALLEGRO_EVENT_DISPLAY_CLOSE:
                state = QUIT;
                break;

            case ALLEGRO_EVENT_KEY_DOWN:
                if (event.keyboard.keycode == ALLEGRO_KEY_ESCAPE) {
                    if (state == PLAYING) state = MENU;
                    else state = QUIT;
                }
                break;

            case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
                if (event.mouse.button == 1) {
                    int mx = event.mouse.x;
                    int my = event.mouse.y;

                    if (state == MENU) {
                        if (mx >= 350 && mx <= 450 && my >= 300
                            && my <= 350) {
                            reset_game();
                            state = PLAYING;
                        } else if (mx >= 350 && mx <= 450 && my >= 400
                            && my <= 450) {
                            state = QUIT;
                        }
                    } else if (state == PLAYING) {
                        handle_board_click(mx, my);
                    }
                }
                break;
            default: ;
        }

        if (redraw && al_is_event_queue_empty(queue)) {
            if (state == MENU) draw_menu();
            else if (state == PLAYING) draw_board();
            redraw = false;
        }
    }

    exit(EXIT_SUCCESS);
error:
    exit(EXIT_FAILURE);
}
