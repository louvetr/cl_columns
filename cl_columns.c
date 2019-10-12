#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#include <wchar.h>
#include <curses.h>
#include <wctype.h>

/////////////////////////////////////////////////////////////////////
// defines
/////////////////////////////////////////////////////////////////////

// game info
#define GTICK 50000 // time (in usec) the game info is refreshed
#define PIECES_LEVEL_TRESHOLD                                                  \
	20 // increment level everytime this nb of pieces is cleared
#define DEFAULT_GAME_PERIOD                                                    \
	750000 // default time value in usec it takes for a piece to fall

// playground size
#define pg_max_x 8
#define pg_max_y 14

// min terminal size
#define term_max_x 80
#define term_max_y 30

// charaters
#define c_border '#'
#define c_exclam '!'
#define c_filler '.'
#define c_empty ' '

// game title in ascii art
#define header_nb 3
#define header_len 22
#define header_0 "╔═╗╔═╗╦  ╦ ╦╔╦╗╔╗╔╔═╗"
#define header_1 "║  ║ ║║  ║ ║║║║║║║╚═╗"
#define header_2 "╚═╝╚═╝╩═╝╚═╝╩ ╩╝╚╝╚═╝"

// control info
#define control_nb 5
#define control_len 15
#define control_0 "z:      toggle"
#define control_1 "q:   move left"
#define control_2 "d:  move right"
#define control_3 "s: faster fall"
#define control_4 "o:        quit"

/////////////////////////////////////////////////////////////////////
// global variables
/////////////////////////////////////////////////////////////////////

// http://www.unicode.org/charts/PDF/U2300.pdf
const wchar_t pieces[] = { L'\x2338' /*, L'\x2339'*/,
			   L'\x233A',
			   L'\x2395',
			   L'\x2341',
			   L'\x2588',
			   L'\x2591' };
wchar_t trio[3] = { 0 };
wchar_t trio_next[3] = { 0 };
static int max_x, max_y;
static int score = 0, nb_pieces_erased = 0;
static wchar_t *screen = NULL;

/////////////////////////////////////////////////////////////////////
// static functions
/////////////////////////////////////////////////////////////////////

// set an message box bordered by 'box-drawing' unicode characters
static int set_msg_box(int x, int y, int x_size, int y_size, char *title,
		       wchar_t **msg_pp, int msg_line)
{
	int i, j, msg_len;
	int title_len = strlen(title);
	wchar_t *msg;

	// corners
	screen[x + y * max_x] = L'\x2554';
	screen[x + (y + 1 + y_size) * max_x] = L'\x255A';
	screen[x + x_size + 1 + y * max_x] = L'\x2557';
	screen[x + x_size + 1 + (y + 1 + y_size) * max_x] = L'\x255D';
	// sides
	for (i = 0; i < y_size; i++) {
		screen[x + (y + 1 + i) * max_x] = L'\x2551';
		screen[x + x_size + 2 - 1 + (y + 1 + i) * max_x] = L'\x2551';
	}
	// lines
	for (i = 1; i < x_size + 1; i++) {
		screen[x + i + (y + 1 + y_size) * max_x] = L'\x2550';
		if (i > 1 && i < title_len + 2)
			screen[x + i + y * max_x] = title[i - 2];
		else
			screen[x + i + y * max_x] = L'\x2550';

		for (j = 0; j < y_size; j++) {
			if (y_size > 1)
				msg = (wchar_t *)msg_pp[j];
			else
				msg = (wchar_t *)msg_pp;
			msg_len = wcslen(msg);
			screen[x + i + (y + 1 + j) * max_x] = c_empty;
			if (i >= x_size + 1 - msg_len && i < x_size + 1)
				screen[x + i - 1 + (y + 1 + j) * max_x] =
					msg[i - (x_size + 1 - msg_len)];
		}
	}

	return 0;
}

// print title of the game
static int display_title(int x, int y)
{
	int i;
	wchar_t *header_wc[header_nb];

	for (i = 0; i < header_nb; i++) {
		header_wc[i] = calloc(sizeof(wchar_t), header_len);
	}
	swprintf(header_wc[0], header_len, L"%s", header_0);
	swprintf(header_wc[1], header_len, L"%s", header_1);
	swprintf(header_wc[2], header_len, L"%s", header_2);
	set_msg_box(x, y, header_len + 1, header_nb, "", (wchar_t **)&header_wc,
		    header_nb);
	for (i = 0; i < header_nb; i++) {
		free(header_wc[i]);
	}
	return 0;
}

// print controls of the game
static int display_controls(int x, int y)
{
	int i;
	wchar_t *control_wc[control_nb];

	for (i = 0; i < control_nb; i++) {
		control_wc[i] = calloc(sizeof(wchar_t), control_len);
	}
	swprintf(control_wc[0], control_len, L"%s", control_0);
	swprintf(control_wc[1], control_len, L"%s", control_1);
	swprintf(control_wc[2], control_len, L"%s", control_2);
	swprintf(control_wc[3], control_len, L"%s", control_3);
	swprintf(control_wc[4], control_len, L"%s", control_4);
	set_msg_box(x, y, control_len + 1, control_nb,
		    "Controls:", (wchar_t **)&control_wc, control_nb);
	for (i = 0; i < control_nb; i++) {
		free(control_wc[i]);
	}
	return 0;
}

// copy playground into screen buffer
static int put_pg_into_screen(wchar_t *pg, wchar_t *screen, int x, int y)
{
	int i, j, offset = x + y * max_x;

	for (j = 0; j < pg_max_y; j++) {
		for (i = 0; i < pg_max_x; i++) {
			screen[i + j * max_x + offset] = pg[i + j * pg_max_x];
		}
	}
	return 0;
}

// get a random value in range
static inline int rnd_in_range(int min, int max)
{
	return rand() % (max - min + 1) + min;
}

// generate a random trio
static void rnd_trio(wchar_t *in_trio)
{
	for (int k = 0; k < 3; k++) {
		in_trio[k] = pieces[rnd_in_range(
			0, sizeof(pieces) / sizeof(pieces[0]) - 1)];
	}
}

// switch position of trio elements
static void toggle_trio(wchar_t *in_trio)
{
	wchar_t tmp = in_trio[2];
	in_trio[2] = in_trio[1];
	in_trio[1] = in_trio[0];
	in_trio[0] = tmp;
}

// delete a trio from playground
static void clear_trio_in_pg(wchar_t *pg, int trio_x, int trio_y)
{
	for (int k = 0; k < 3; k++)
		pg[trio_x + (trio_y - k) * pg_max_x] = c_empty;
}

// count the number of pieces missing from a pg to another
static int get_nb_pieces_diff(wchar_t *pg1, wchar_t *pg2)
{
	int i, j, diff, cpt1 = 0, cpt2 = 0;

	for (j = 0; j < pg_max_y; j++) {
		for (i = 0; i < pg_max_x; i++) {
			int k = i + j * pg_max_x;
			if (pg1[k] != c_empty && pg1[k] != c_border)
				cpt1++;
			if (pg2[k] != c_empty && pg2[k] != c_border)
				cpt2++;
		}
	}
	diff = cpt1 - cpt2;
	if (diff < 0)
		diff = -diff;

	return diff;
}

// all pieces goes down if they are above a empty slot
static int apply_gravity(wchar_t *pg_in, wchar_t *pg_out)
{
	int x, y, cpt;

	// init an empty pg screen
	for (x = 1; x < pg_max_x - 1; x++) {
		for (y = 0; y < pg_max_y - 1; y++) {
			pg_out[x + y * pg_max_x] = c_empty;
		}
	}

	for (x = 1; x < pg_max_x - 1; x++) {
		cpt = 0;
		// remove the empty slots of each columns and make fall the pieces
		for (y = pg_max_y - 2; y > 1; y--) {
			if (pg_in[x + y * pg_max_x] != c_empty) {
				pg_out[x + (pg_max_y - 2 - cpt) * pg_max_x] =
					pg_in[x + y * pg_max_x];
				cpt++;
			}
		}
	}

	return 0;
}

// detect 3 aligned similar pieces. Destroy then and count score
static int check_combo(wchar_t *pg)
{
	int x, y, cpt, aligned;
	int tmp_score = 0;
	wchar_t pg_copy[pg_max_x * pg_max_y];

	memcpy(pg_copy, pg, pg_max_x * pg_max_y * sizeof(wchar_t));

	for (x = 1; x < pg_max_x - 1; x++) {
		for (y = 0; y < pg_max_y - 1; y++) {
			if (pg[x + y * pg_max_x] == c_empty)
				continue;

			// diagonal bottom left
			for (aligned = 1; x - aligned < pg_max_x - 1 &&
					  y + aligned < pg_max_y - 1;
			     aligned++) {
				if (pg[x + y * pg_max_x] !=
				    pg[x - aligned + (y + aligned) * pg_max_x])
					break;
			}
			if (aligned >= 3) {
				tmp_score += aligned;
				for (cpt = 0; cpt < aligned; cpt++) {
					pg_copy[x - cpt + (y + cpt) * pg_max_x] =
						c_empty;
				}
			}

			// diagonal bottom right
			for (aligned = 1; x + aligned < pg_max_x - 1 &&
					  y + aligned < pg_max_y - 1;
			     aligned++) {
				if (pg[x + y * pg_max_x] !=
				    pg[x + aligned + (y + aligned) * pg_max_x])
					break;
			}
			if (aligned >= 3) {
				tmp_score += aligned;
				for (cpt = 0; cpt < aligned; cpt++) {
					pg_copy[x + cpt + (y + cpt) * pg_max_x] =
						c_empty;
				}
			}

			// horizontal
			for (aligned = 1; x + aligned < pg_max_x - 1;
			     aligned++) {
				if (pg[x + y * pg_max_x] !=
				    pg[x + aligned + y * pg_max_x])
					break;
			}
			if (aligned >= 3) {
				tmp_score += aligned;
				for (cpt = 0; cpt < aligned; cpt++) {
					pg_copy[x + cpt + y * pg_max_x] =
						c_empty;
				}
			}

			// vertical
			for (aligned = 1; y + aligned < pg_max_y - 1;
			     aligned++) {
				if (pg[x + y * pg_max_x] !=
				    pg[x + (y + aligned) * pg_max_x])
					break;
			}
			if (aligned >= 3) {
				tmp_score += aligned;
				for (cpt = 0; cpt < aligned; cpt++) {
					pg_copy[x + (y + cpt) * pg_max_x] =
						c_empty;
				}
			}
		}
	}

	nb_pieces_erased += get_nb_pieces_diff(pg, pg_copy);

	// if combo happened, pieces have to fall
	if (tmp_score > 0) {
		// blink pieces to delete
		for (cpt = 0; cpt < 6; cpt++) {
			put_pg_into_screen(cpt % 2 ? pg_copy : pg, screen,
					   max_x / 2 - pg_max_x / 2,
					   max_y * 3 / 7);
			mvaddwstr(0, 0, screen);
			refresh();
			usleep(100000);
		}
		// pieces falling
		apply_gravity(pg_copy, pg);
	}

	return tmp_score;
}

/////////////////////////////////////////////////////////////////////
// main function
/////////////////////////////////////////////////////////////////////

int main()
{
	int i, j; // counters for loops
	int game_is_over = 0; // flag for game over
	int exit = 0; // flag to quit loop
	int fast_fall = 0; // flag for fast falling of pieces
	int redraw; // flag to tell redrawing screen is needed
	int clock = 0; // time elapsed since last game_cycle
	int game_cycle =
		DEFAULT_GAME_PERIOD; // period of time necessary for a piece to fall.
	int level =
		1; // difficulty of the game. higher it is, shorter is game cycle.
	char coef_s[18];
	wchar_t wstr[32];
	wchar_t *next_wc[3];

	// screen info
	int screen_size, pg_size;
	wchar_t *pg = NULL;

	// coordinate of the lowest block of the falling trio
	int trio_x, trio_y;

	setlocale(LC_ALL, "");
	srand(time(0));

	// NCurses setup
	setlocale(LC_ALL, ""); // Set locale for UTF-8 support
	initscr(); // Initialise NCurses screen
	noecho(); // Don't echo input to screen
	curs_set(0); // Don't show terminal cursor
	nodelay(stdscr, true); // Don't halt program while waiting for input
	cbreak(); // Make input characters immediately available to the program
	getmaxyx(stdscr, max_y, max_x);

	if (max_x < term_max_x || max_y < term_max_y) {
		printf("max_x = %d, max_y = %d\n", max_x, max_y);
		printf("ERROR: terminal size as to be at least 80x30.\n");
		return -1;
	}

	// allocate screen
	screen_size = max_x * max_y + 1;
	screen = calloc(sizeof(wchar_t), screen_size);
	if (screen == NULL) {
		printf("ERROR: calloc failed.\n");
		return -1;
	}
	// init screen
	for (i = 0; i < screen_size - 1; i++) {
		screen[i] = c_filler;
	}

	// allocate playground
	pg_size = pg_max_x * pg_max_y;
	pg = calloc('#', pg_size);
	if (pg == NULL) {
		printf("ERROR: calloc failed.\n");
		return -1;
	}

	// init playground
	for (i = 0; i < pg_size; i++) {
		if (i > pg_size - pg_max_x || i % pg_max_x == 0 ||
		    i % pg_max_x == pg_max_x - 1)
			pg[i] = c_border;
		else
			pg[i] = c_empty;
	}

	rnd_trio(trio_next);

	// game loop
	while (!exit) {
		redraw = 0;
		fast_fall = 0;
		clear();

		// a new trio enter the screen
		//   -Copy it from spoiler
		//   -Generate a new spoiler trio
		if (trio[0] == 0) {
			memcpy(trio, trio_next, sizeof(trio_next));
			rnd_trio(trio_next);
			// initial position of trio
			trio_x = 4;
			trio_y = 1;
			//lose condition
			if (pg[4 + 2 * pg_max_x] != c_empty)
				game_is_over = 1;
		}

		// input =======================================
		int key = getch();
		switch (key) {
		case 'q': // left
			if (pg[trio_x - 1 + trio_y * pg_max_x] == c_empty &&
			    pg[trio_x - 1 + (trio_y + 1) * pg_max_x] ==
				    c_empty) {
				for (int k = 0; k < 3; k++)
					pg[trio_x - 1 + (trio_y - k) * pg_max_x] =
						pg[trio_x +
						   (trio_y - k) * pg_max_x];

				clear_trio_in_pg(pg, trio_x, trio_y);
				trio_x--;

				redraw = 1;
			}
			break;
		case 'd': // right
			if (pg[trio_x + 1 + trio_y * pg_max_x] == c_empty &&
			    pg[trio_x + 1 + (trio_y + 1) * pg_max_x] ==
				    c_empty) {
				for (int k = 0; k < 3; k++)
					pg[trio_x + 1 + (trio_y - k) * pg_max_x] =
						pg[trio_x +
						   (trio_y - k) * pg_max_x];

				clear_trio_in_pg(pg, trio_x, trio_y);
				trio_x++;
				redraw = 1;
			}
			break;
		case 's': // down
			if (pg[trio_x + (trio_y + 1) * pg_max_x] == c_empty) {
				fast_fall = 1;
				mvaddstr(1, 1, "fast_fall set to 1");
				redraw = 1;
			}
			break;
		case 'z': // up
			toggle_trio(trio);
			break;
		case 'o': // exit
			exit = 1;
			break;
		default: // no input, snake goes ahead
			break;
		}

		clock += GTICK;

		if (clock >= game_cycle || fast_fall) {
			clock = 0;

			// check trio collision
			if (pg[trio_x + (trio_y + 1) * pg_max_x] == c_empty) {
				pg[trio_x + (trio_y + 1) * pg_max_x] = trio[2];
				pg[trio_x + (trio_y)*pg_max_x] = trio[1];
				pg[trio_x + (trio_y - 1) * pg_max_x] = trio[0];
				pg[trio_x + (trio_y - 2) * pg_max_x] = c_empty;
				trio_y++;
			} else {
				int tmp_score = 0, coeff_score = 1;
				do {
					tmp_score = check_combo(pg);
					score += tmp_score * coeff_score;
					// update level and game cycle accordingly
					level = nb_pieces_erased /
							PIECES_LEVEL_TRESHOLD +
						1;
					if (level >=
					    DEFAULT_GAME_PERIOD / GTICK)
						level = DEFAULT_GAME_PERIOD /
								GTICK -
							1;
					game_cycle = DEFAULT_GAME_PERIOD -
						     GTICK * level;

					if (coeff_score > 1) {
						snprintf(coef_s, 32,
							 "COMBO X %d",
							 coeff_score);
						/*for (i = 0; i < strlen(coef_s); i++)
						screen[i] = coef_s[i];*/
						mvaddstr(3, 0, coef_s);
						refresh();
					}
					coeff_score++;
				} while (tmp_score > 0);
				trio[0] = 0;
				fast_fall = 0;
			}
		}

		// change backgrount
		if (game_is_over) {
			for (j = 0; j < max_y; j++) {
				for (i = 0; i < max_x; i++)
					screen[i + j * max_x] = c_exclam;
			}
		}

		// next trio spoiler
		for (i = 0; i < 3; i++) {
			next_wc[i] = calloc(sizeof(wchar_t), 2);
			next_wc[i][0] = trio_next[2 - i];
		}
		set_msg_box(max_x * 2 / 7, max_y * 2 / 5, 6, 3,
			    "Next:", next_wc, 3);
		// TODO: beware of the free
		for (i = 0; i < 3; i++) {
			free(next_wc[i]);
		}

		// display control
		display_controls(max_x / 4 - control_len / 2, max_y * 2 / 3);

		// put playground into screen
		put_pg_into_screen(pg, screen, max_x / 2 - pg_max_x / 2,
				   max_y * 3 / 7);

		// game title
		display_title(max_x / 2 - header_len / 2, max_y / 8);

		// score
		swprintf(wstr, 20, L"%d pts", score);
		set_msg_box(max_x * 2 / 3, max_y * 2 / 6, 16, 1,
			    "Score:", (wchar_t **)&wstr, 1);

		// extra info about score (combo, sub-score)
		swprintf(wstr, 20, L"%s", "TODO");
		set_msg_box(max_x * 2 / 3, max_y * 3 / 6, 16, 1,
			    "Info:", (wchar_t **)&wstr, 1);

		// level
		swprintf(wstr, 20, L"%d", level);
		set_msg_box(max_x * 2 / 3, max_y * 4 / 6, 16, 1,
			    "Level:", (wchar_t **)&wstr, 1);

		// score in pieces
		swprintf(wstr, 20, L"%d", nb_pieces_erased);
		set_msg_box(max_x * 2 / 3, max_y * 5 / 6, 16, 1,
			    "Pieces:", (wchar_t **)&wstr, 1);

		if (game_is_over) {
			swprintf(wstr, 20, L"%s", "GAME OVER");
			set_msg_box(max_x / 2 - 6, max_y / 2, 11, 1, "",
				    (wchar_t **)&wstr, 1);
		}

		// print the screen
		mvaddwstr(0, 0, screen);
		refresh();

		if (!redraw || !fast_fall)
			usleep(GTICK);
	}

	endwin();
	free(screen);
	free(pg);

	return 0;
}
