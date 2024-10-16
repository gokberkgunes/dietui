#include <limits.h>
#include <locale.h>
#include <ncurses.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>

#define MAX(A, B) ((A) > (B) ? (A) : (B))
#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define BETWEEN(X, A, B) ((A) <= (X) && (X) <= (B))

/*
 * COLOR_BLACK   0
 * COLOR_RED     1
 * COLOR_GREEN   2
 * COLOR_YELLOW  3
 * COLOR_BLUE    4
 * COLOR_MAGENTA 5
 * COLOR_CYAN    6
 * COLOR_WHITE   7
 */

#define CLR_RED 8
#define CLR_GRN 9
#define CLR_YEL 10
#define CLR_BLU 11
#define CLR_CYN 12
#define CLR_PUR 13
#define CLR_ORG 14
#define CLR_GRY 15
#define CLR_BG0 16
#define CLR_BG1 17
#define CLR_BG2 18

#define KEYSPACE 32
#define KEYDEL   80
#define KEYESC   27

static const int N_WINDOWS    = 2;
static const int N_POPUPS     = 2;
static const int WIN_0_WIDTH  = 20;
static const int POPUP_0_H    = 10;
static const int POPUP_0_W    = 30;
static       int selwin       = 0;
static       int tmp          = 0;
             int CHARSIZE     = 3; // strlen("─"); // ─ is 3 bytes long.
             int NFOOD;            // number of food, or nline in foods

// box drawing chracter. 40 ch long since printf will format it.
const char BOXCHR[] = "────────────────────────────────────────";

typedef struct {
	char name[31];
	//double gram, kcal, carb, fat, prot, fiber;
	double weight, kcal, carb, fat, prot, fiber;
} iteminfo;

typedef struct {
	int index;
	int h, w, y, x;
	int curline;
	int start;
	int nitem;
	int menuh;
} winprop;

/* Crude conversation might be done in initclrs(), 1000/255 ~= 4. */
void
initclrs(void)
{
	init_color(CLR_RED, 882, 208, 16);                 /* #e13504 */
	init_color(CLR_GRN, 55, 800, 431);                 /* #0ecc6e */
	init_color(CLR_YEL, 0, 831, 4);                    /* #fdd401 */
	init_color(CLR_BLU, 302, 549, 98);                 /* #4d8cfe */
	init_color(CLR_CYN, 259, 537, 631);                /* #4289a1 */
	init_color(CLR_PUR, 592, 368, 654);                /* #975ea7 */
	init_color(CLR_ORG, 1000, 533, 0);                 /* #ff8800 */
	init_color(CLR_GRY, 502, 502, 502);                /* #808080 */
	init_color(CLR_BG0, 0x12 * 4, 0x12 * 4, 0x12 * 4); /* #121212 */
	init_color(CLR_BG1, 0x1c * 4, 0x1c * 4, 0x1c * 4); /* #1c1c1c */
	init_color(CLR_BG2, 0x2c * 4, 0x2c * 4, 0x2c * 4); /* #2c2c2c */
	init_pair(1, COLOR_WHITE, CLR_BG0);
	init_pair(2, COLOR_WHITE, CLR_BG1);
	init_pair(3, COLOR_WHITE, CLR_BG2);
}

void
wmvcursor(WINDOW *win, winprop *winfo, int y, int x)
{
	if (x > 0)
		winfo->x = MIN(winfo->x + x, winfo->w - 1);
	else
		winfo->x = MAX(winfo->x + x, 0);

	if (y > 0)
		winfo->y = MIN(winfo->y + y, winfo->h - 1);
	else
		winfo->y = MAX(winfo->y + y, 0);

	wmove(win, winfo->y, winfo->x);
}

int
chr_array_to_num(char array[], int size)
{
	int result = 0;
	int num = 0;
	for (int i = 0; i < size && array[i] != '\0' ; i++) {
		num = array[i] - '0';
		result += num*(10*(i+1));
	}
	return num;
}


/* Calculator of the window's index base on a distance variable.
 * Circulates in 0, 1, ..., N_WINDOWS-1.
 */
int
selcalc(int distance)
{
	return (selwin + N_WINDOWS + distance%N_WINDOWS)%N_WINDOWS;
}

void
jmpwin(int distance)
{
	selwin = selcalc(distance);
}

void
nextwin(void)
{
	jmpwin(+1);
}
void
prevwin(void)
{
	jmpwin(-1);
}

void
delitem(iteminfo *item, winprop* winfo)
{
	/* Shift all the items to cover the deleted item's position */
	for (int i = winfo->curline; i < winfo->nitem-1; i++)
		item[i] = item[i+1];

	/* Decrease number of items and currlent up to 0 */
	if (winfo->nitem > 0) {
		winfo->nitem--;
	} else {
		winfo->nitem = 0;
        }
	/* If there are more items in the window, scroll towards start */
	if (winfo->curline > 0) {
		winfo->curline--;
	} else {
		winfo->curline = 0;
	}
}

void
moditem(iteminfo *item, long number)
{
	double weight_ratio = number/item->weight;

	item->weight = number;
	item->kcal   = item->kcal*weight_ratio;
	item->carb   = item->carb*weight_ratio;
	item->fat    = item->fat*weight_ratio;
	item->prot   = item->prot*weight_ratio;
	item->fiber  = item->fiber*weight_ratio;
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Function    :      nviswide
 *
 * Description :      Count visible wide and narrow characters for a
 *                    printf-like input. This does not count bytes.
 *                    Acts similar to snprintf(NULL, 0, fmt, args) but also
 *                    takes wide chars. Written due to the lack of snwprintf.
 *
 * Arguments   :      const char *fmt: printf-like formatting
 *                    ...: printf-like variadic arguments
 *
 * Return      :      Number of wide and narrow characters as int
 *
 * Error       :      Exits with -1 on failure, could be replaced to return -1.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
int nvischr(const char *fmt, ...)
{
	va_list args, args2;
	va_start(args, fmt);
	va_copy(args2,args); /* We cannot use va twice */

	/* CALCULATE THE LENGTH, WITHOUT '\0', OF FORMATTED STRING */
	int len = vsnprintf(NULL, 0, fmt, args);
	va_end(args);

	if (len < 0) {
		va_end(args2);
		fprintf(stdout, "%s\n", "ERROR: formatting failed.");
		exit(-1);
	}

	/* generate string and allocate the formatted string */
	char tmp[len+1];
	vsnprintf(tmp, len+1, fmt, args2);
	va_end(args2);

	size_t nvis = mbstowcs(NULL, tmp, 0);

	/* As implied in the documentation, mbstowcs returns (size_t)-1 if an
	 * invalid sequence given.
	 */
	if (nvis == (size_t)-1) {
		fprintf(stdout, "%s\n", "ERROR: could not count wide chars.");
		exit(1);
	}

	/* Usage of size_t is cumbersome, transform to int. */
	if (nvis > INT_MAX) {
		return INT_MAX;
	}
	return (int)nvis;
}

void
drawmenu(WINDOW *win, iteminfo *item, winprop* winfo)
{
	int winwidth   = winfo->w;
	int start      = MAX(winfo->start, 0);
	int highlight  = (selwin == winfo->index) ? winfo->curline : -1;
	int menuheight = winfo->menuh;
	int nitem      = winfo->nitem;

	int pad;
	int limit = MIN((start+menuheight), nitem); /* ensure list fits */

	wclear(win);
	for (int i = start; i < limit; i++) {
		if (i == highlight) {
			/* Calculate formatted string's length by passing size
			* of buffer as 0 pointing to NULL. This is a safe
			* operation as explained by the reference below.
			* https://unix.stackexchange.com/a/713067
			*/
			pad = winwidth - snprintf(NULL, 0, "%s", item[i].name);
			wattron(win, A_REVERSE);
			wprintw(win, "%s%*s", item[i].name, pad, "");
			wattroff(win, A_REVERSE);
			continue;
		}
		wprintw(win, "%s\n", item[i].name);
	}
	wrefresh(win);
}

void
draw_menu_header(WINDOW *win, unsigned int len[7])
{

	wprintw(win, "%-*s│%*s│%*s│%*s│%*s│%*s│%*s│\n",
		len[0], "NAME",
		len[1], "GRAM",
		len[2], "KCAL",
		len[3], "CARB",
		len[4], "FAT",
		len[5], "PROT",
		len[6], "FIBER"
	);

	wprintw(win, "%-.*s┼%.*s┼%.*s┼%.*s┼%.*s┼%.*s┼%.*s┤\n",
		len[0]*CHARSIZE, BOXCHR,
		len[1]*CHARSIZE, BOXCHR,
		len[2]*CHARSIZE, BOXCHR,
		len[3]*CHARSIZE, BOXCHR,
		len[4]*CHARSIZE, BOXCHR,
		len[5]*CHARSIZE, BOXCHR,
		len[6]*CHARSIZE, BOXCHR
	);
}
void
draw_menu_item(WINDOW *win, iteminfo item, int highlight, unsigned int len[7])
{
	if (highlight)
		wattron(win, A_REVERSE);
	wprintw(win, "%-*s│%*.0lf│%*.0lf│%*.0lf│%*.0lf│%*.0lf│%*.0lf│\n",
		len[0], item.name,
		len[1], item.weight,
		len[2], item.kcal,
		len[3], item.carb,
		len[4], item.fat,
		len[5], item.prot,
		len[6], item.fiber
	);
	wattroff(win, A_REVERSE);
}

void
draw_menu_footer(WINDOW *win, unsigned int len[7], double total[5])
{

	wprintw(win, "%-.*s┴%.*s┼%.*s┼%.*s┼%.*s┼%.*s┼%.*s┤\n",
		len[0]*CHARSIZE, BOXCHR,
		len[1]*CHARSIZE, BOXCHR,
		len[2]*CHARSIZE, BOXCHR,
		len[3]*CHARSIZE, BOXCHR,
		len[4]*CHARSIZE, BOXCHR,
		len[5]*CHARSIZE, BOXCHR,
		len[6]*CHARSIZE, BOXCHR
	);

	wprintw(win, "%-*s│%*.0lf│%*.0lf│%*.0lf│%*.0lf│%*.0lf│\n",
		len[0]+len[1]+1, "TOTAL",
		len[2],          total[0],
		len[3],          total[1],
		len[4],          total[2],
		len[5],          total[3],
		len[6],          total[4]
	);
}


void drawmain(WINDOW *win, iteminfo *item, winprop* winfo)
{
	int start      = MAX(winfo->start, 0);
	int highlight  = (selwin == winfo->index) ? winfo->curline : -1;
	int menuheight = winfo->menuh;
	int nitem      = winfo->nitem;

	int limit = MIN((start+menuheight), nitem); /* ensure list fits */

	/* iteminfo's variables */
	double total[5] = {0, 0, 0, 0, 0};
	// space, weight, kcal, carb, protein, fiber
	unsigned int print_len[7] = {20, 6, 6, 5, 4, 4, 6};

	/* TODO: we need a function to properly draw a table, dynamic column */
	wclear(win);

	/* HEADER */
	draw_menu_header(win, print_len);

	for (int i = start; i < limit; i++) {
		total[0] += item[i].kcal;  /* 5 digits due to sum */
		total[1] += item[i].carb;  /* 4 digits */
		total[2] += item[i].fat;   /* 4 digits */
		total[3] += item[i].prot;  /* 4 digits */
		total[4] += item[i].fiber; /* 4 digits */
		draw_menu_item(win, item[i], i == highlight, print_len);
	}
	/* footer */
	draw_menu_footer(win, print_len, total);
	/* refresh window to reflect changes */
	wrefresh(win);
}

void
copyitem(iteminfo *all, iteminfo *saved, winprop *winall, winprop *winsaved)
{
	/* Check if name2add is in saved items. */
	for (int i = 0; i < winsaved->nitem; i++)
	       if (!strcmp(saved[i].name, all[winall->curline].name))
		       return;

	/* If we found no duplicates: Add the new item, then increase number of
	 * items. No need to check for number of items exceeds the total number
	 * since there are no duplicates.
	 */
	memcpy(&saved[winsaved->nitem++], &all[winall->curline], sizeof(iteminfo));
	/* Increase menu size to number of items in it */
	winsaved->menuh = (winsaved->nitem >= LINES) ? LINES : winsaved->nitem;
}
int
countlines(FILE *fpath)
{
	int chr, nline = 0;
	while ((chr = fgetc(fpath)) != EOF)
		if (chr == '\n')
			nline++;
	/* seek back to start of the file */
	fseek(fpath, 0, SEEK_SET);
	return nline;
}

void
readlines(int n, char ***linearr, size_t *bufsize, FILE *fp)
{
	ssize_t nchars; /* number of read characters per line */
	int lineno;

	/* Skip the first line */
	fscanf(fp, "%*[^\n]\n");

	for (lineno = 0; lineno < n; lineno++) {
		nchars = getline(&(*linearr)[lineno], bufsize, fp);

		if (nchars < 0)
			break;

		/* getline() reads newline character, remove it */
		if ((*linearr)[lineno][nchars - 1] == '\n')
			(*linearr)[lineno][nchars - 1] = '\0';
	}
	if (lineno == 1 && nchars < 0)
		exit(1);
	// die("Given file has no lines to read.");
}

void
getitemdata(iteminfo *f, char **str, int nlines)
{
	/* go through file to set data */
	for (int i = 0; i < nlines; i++) {
		sscanf(str[i], "%30[^,],%lf,%lf,%lf,%lf,%lf", f[i].name,
		       &f[i].kcal, &f[i].carb, &f[i].fat, &f[i].prot,
		       &f[i].fiber);
		f[i].weight = 100; // Set initial weight to 100gr.
	}
}

iteminfo*
readfoodfile(void)
{
	FILE *fp = NULL;
	size_t bufsize = 100; /* initial malloc string length */

	fp = fopen("./data", "r");
	if (fp == NULL) {
		endwin(); /* free memory, quit ncurses */
		fprintf(stderr, "ERROR: cannot open the file.\n");
		exit(1);
	}

	/* count number of lines in the file, minus the header */
	NFOOD = countlines(fp) - 1;

	iteminfo *item = (iteminfo *)malloc(NFOOD * sizeof(iteminfo));

	/* seek back to start of the file */
	fseek(fp, 0, SEEK_SET);

	/* Generate data structure to hold file's contents */
	char *filedata[NFOOD];

	for (int j = 0; j < NFOOD; j++)
		filedata[j] = malloc(200 * sizeof(char));

	char **ptr = filedata;
	readlines(NFOOD, &ptr, &bufsize, fp);

	getitemdata(item, filedata, NFOOD);

	/* free dynamically allocated memories used by getline call */
	for (int j = 0; j < NFOOD; j++)
		free(filedata[j]);
	fclose(fp);

	return item;
}

int
main(void)
{
	char input[31];
	long number;
	wint_t key = 1;

	setlocale(LC_ALL, ""); /* enable long char before ncurses calls. */
	initscr(); /* initialize ncurses: memory & clear screen */
	noecho(); /* do not write keypresses to screen */
	start_color(); /* enable color support */
	initclrs(); /* initialize custom colors */
	curs_set(0); /* cursor, 0: invis, 1: blinking, 2: solid */
	ESCDELAY = 250; /* delay time to 250 ms for esc key */
	keypad(stdscr, TRUE); /* enable keypad to avoid escape sequences */


	winprop winfo[N_WINDOWS];
	winprop pinfo[N_POPUPS];
	WINDOW *win[N_WINDOWS];
	WINDOW *popup[N_POPUPS];
	iteminfo *item[2];

	item[0] = readfoodfile();
	item[1] = (iteminfo *)malloc(NFOOD * sizeof(iteminfo));

	/*                    no, height, width,           y, x,          curline, start, nitem, hghtmenu */
	winfo[0] = (winprop){ 0,  LINES,  WIN_0_WIDTH,     0, 0,          0,       0,     NFOOD, 0 };
	winfo[1] = (winprop){ 1,  LINES,  COLS-winfo[0].w, 0, winfo[0].w, 0,       0,     0,     0 };
	pinfo[0] = (winprop){
		2,                              // window number
		POPUP_0_H,                      // height
		POPUP_0_W,                      // width
		(LINES-POPUP_0_H)/2,            // y coordinate
		(WIN_0_WIDTH+COLS-POPUP_0_W)/2, // x coordinate
		0,                              // current line number
		0,                              // starting number
		0,                              // number of items
		0                               // is highlighted
	};

	win[0] = newwin(winfo[0].h, winfo[0].w, winfo[0].y, winfo[0].x);
	win[1] = newwin(winfo[1].h, winfo[1].w, winfo[1].y, winfo[1].x);
	popup[0] = newwin(pinfo[0].h, pinfo[0].w, pinfo[0].y, pinfo[0].x);
	winfo[selwin].menuh = (winfo[selwin].nitem > LINES) ? LINES : winfo[selwin].nitem;

	wbkgd(win[0], COLOR_PAIR(1)); /* set color of win[0] */
	wbkgd(win[1], COLOR_PAIR(2)); /* set color of win[1] */
	wbkgd(popup[0], COLOR_PAIR(3));

	/* We must call this after window decleration. */
	refresh();
	wrefresh(win[0]);
	wrefresh(win[1]);


	/* Initially draw the main window */
	drawmenu(win[selwin], item[selwin], &winfo[selwin]);

	/* TODO: put this loop into a function, function should exit when '\t'
	 * is given and update active variable. Therefore we can change to
	 * another window this way.
	 */
	while (key) {

		key = getch();

		switch (key) {
		case 'h':
			break;
		case 'j':
		case KEY_DOWN:
			/* Draw scrolling menu */
			winfo[selwin].curline += 1;
			winfo[selwin].menuh = (winfo[selwin].nitem > LINES) ? LINES : winfo[selwin].nitem;

			if (winfo[selwin].curline >= winfo[selwin].nitem)
				winfo[selwin].start = winfo[selwin].curline = 0;
			else if (winfo[selwin].curline >= winfo[selwin].menuh + winfo[selwin].start)
				winfo[selwin].start += winfo[selwin].menuh;
			if (selwin == 1)
				drawmain(win[selwin], item[selwin], &winfo[selwin]);
			else
				drawmenu(win[selwin], item[selwin], &winfo[selwin]);
			break;
		case 'k':
		case KEY_UP:
			/* Draw scrolling menu */
			winfo[selwin].curline -= 1;
			winfo[selwin].menuh = (winfo[selwin].nitem >= LINES) ? LINES : winfo[selwin].nitem;

			if (winfo[selwin].curline < 0) {
				winfo[selwin].start = (winfo[selwin].nitem - winfo[selwin].menuh);
				winfo[selwin].curline = winfo[selwin].nitem - 1;
			} else if (winfo[selwin].curline < winfo[selwin].start)
				winfo[selwin].start -= winfo[selwin].menuh;

			if (selwin == 1)
				drawmain(win[selwin], item[selwin], &winfo[selwin]);
			else
				drawmenu(win[selwin], item[selwin], &winfo[selwin]);
			break;
		case 'l':
			break;
		case 'r':
			clear();
			break;
		case 'p':
			// pause();
			break;
		case KEYDEL: /* FALLTHROUGH */
		case 'd':
			if (selwin == 1)
				delitem(item[1], &winfo[1]);
			drawmenu(win[1], item[1], &winfo[1]);
			if (winfo[1].nitem == 0) {
				nextwin();
				drawmenu(win[selwin], item[selwin], &winfo[selwin]);
			}
			break;
		case KEYESC: /* FALLTHROUGH */
		case 'q': /* FALLTHROUGH */
		case 'Q':
			key = 0x0;
			break;
		case '\t':
		case KEY_LEFT:
		case KEY_RIGHT:
			tmp = selwin;
			nextwin();
			/* Redraw last window without highlights*/
			drawmenu(win[tmp], item[tmp], &winfo[tmp]);
			if (selwin == 1)
				drawmain(win[selwin], item[selwin], &winfo[selwin]);
			else
				drawmenu(win[selwin], item[selwin], &winfo[selwin]);
			break;
		case '\n':
		case KEYSPACE:
			if (selwin == 0) {
				/* copy selected data */
				copyitem(item[0], item[1], &winfo[0], &winfo[1]);
				drawmenu(win[1], item[1], &winfo[1]);
			} else if (selwin == 1) {
				/* TODO: on right window ask for how much grams,
				 * then calculate values accordingly also write
				 * 100gr initially */

				mvwprintw(popup[0], 1, 1, "%s", item[selwin][winfo[selwin].curline].name);
				mvwprintw(popup[0], 2, 1, "How many grams?");
				box(popup[0], 0, 0);

				wattron(popup[0], COLOR_PAIR(1));  // Turn on the color pair for printing text
				mvwprintw(popup[0], 4, 1, "%-*s", 5, "");

				wmove(popup[0], 4, 1);
				curs_set(1); /* cursor, 0: invis, 1: blinking, 2: solid */
				/* set cursor to thin, works for st */
				printf("\033[6 q");

				wrefresh(popup[0]);

				for (int i = 0, maxlen = 4;;) {
					key = getch();

					if (key == '\n' || key == 'q' ||
					    key == KEYESC)
						break;

					if (i > 0 && key == KEY_BACKSPACE) {
						//input[--i] = ' ';
						input[--i] = '\0';
						mvwprintw(popup[0], 4, 1,"%-*s", i+1, input);
						wmove(popup[0], 4, 1+i);
						//wclear(popup[0]);
						wrefresh(popup[0]);
						continue;
					} else if (i == maxlen) {
						//input[--i] = '\0';
						continue;
					}

					if (key >= '0' && key <= '9') {
						input[i]   = key;
						input[++i] = '\0';
						mvwprintw(popup[0], 4, 1,"%s", input);
						wrefresh(popup[0]);
						continue;
					}
				}
				number = strtol(input, NULL, 10);
				if (number > 0)
					moditem(&item[1][winfo[1].curline], number);

				wattroff(popup[0], COLOR_PAIR(1)); // Turn off the color pair
				wclear(popup[0]);
				curs_set(0); /* hide cursor */
				/* Redraw the old window */
				drawmain(win[1], item[1], &winfo[1]);
			}
			break;
		default:
			break;
		}
	}
	delwin(win[0]);
	delwin(win[1]);
	endwin(); /* free memory, quit ncurses */
	free(item[0]);
	free(item[1]);
	return 0;
}
