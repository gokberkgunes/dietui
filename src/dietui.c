#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <wchar.h>
#include <locale.h>

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

#define KEYSPACE 32
#define KEYDEL   80
#define KEYESC   27

static const int N_WINDOWS   = 2;
static const int WIN_0_WIDTH = 30;
static       int selwin      = 0;
static       int tmp         = 0;

typedef struct {
	char name[31];
	double kcal, carb, fat, prot, fiber;
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
	init_pair(1, COLOR_WHITE, CLR_BG0);
	init_pair(2, COLOR_WHITE, CLR_BG1);
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
drawmenu(WINDOW *win, iteminfo *item, winprop* winfo)
{
	int winwidth   =  winfo->w;
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

void drwmain(WINDOW *win, iteminfo *item, winprop* winfo)
{
	int winwidth   =  winfo->w;
	int start      = MAX(winfo->start, 0);
	int highlight  = (selwin == winfo->index) ? winfo->curline : -1;
	int menuheight = winfo->menuh;
	int nitem      = winfo->nitem;

	int limit = MIN((start+menuheight), nitem); /* ensure list fits */

	/* iteminfo's variables */
	char *name;
	double val[5], tot[5] = { 0, 0, 0, 0, 0};

	/* TODO: we need a function to properly draw a table, dynamic column */
	wclear(win);

	/* HEADER */
	wprintw(win, "%-30s│ %6s │ %4s │ %4s │ %4s │ %4s │\n", "NAME", "KCAL",
		"CARB", "FAT", "PROT", "FIBER");

	/* TODO: Wrap this to a function as wprintww */
	wchar_t header[winwidth+1];
        swprintf(header, winwidth+1, L"%ls\n",
                 L"──────────────────────────────┼────────┼──────┼──────┼────"
                 L"──┼───────┤");
        waddwstr(win, header);
	/********************************************/

	for (int i = start; i < limit; i++) {
		name   = item[i].name;
		val[0] = item[i].kcal;  /* 5 digits due to sum */
		val[1] = item[i].carb;  /* 4 digits */
		val[2] = item[i].fat;   /* 4 digits */
		val[3] = item[i].prot;  /* 4 digits */
		val[4] = item[i].fiber; /* 4 digits */

		for (int i = 0; i < 5; i++)
			tot[i] += val[i];

		if (i == highlight) {
			wattron(win, A_REVERSE);
			wprintw(win, "%-30s│ %6.0lf │ %4.1lf │ %4.1lf │ %4.1lf │ %4.1lf  │\n", name,val[0],val[1],val[2],val[3],val[4]);
			wattroff(win, A_REVERSE);
			continue;
		}
		wprintw(win, "%-30s│ %6.0lf │ %4.1lf │ %4.1lf │ %4.1lf │ %4.1lf  │\n", name,val[0],val[1],val[2],val[3],val[4]);
	}
	/* footer */
	waddwstr(win, header);
	wprintw(win, "%-30s│ %6.0lf │ %4.1lf │ %4.1lf │ %4.1lf │ %4.1lf  │\n", "TOTAL",tot[0],tot[1],tot[2],tot[3],tot[4]);
	/* refresh window to reflect changes */
	wrefresh(win);
}

void
copyitem(iteminfo *allitem, iteminfo *saveditem, int highlight, int n_saved)
{
	memcpy(&saveditem[n_saved], &allitem[highlight], sizeof(iteminfo));
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
	for (int i = 0; i < nlines; i++)
		sscanf(str[i], "%30[^,],%lf,%lf,%lf,%lf,%lf", f[i].name,
		       &f[i].kcal, &f[i].carb, &f[i].fat, &f[i].prot,
		       &f[i].fiber);
}

iteminfo *
getiteminfo(int *nline)
{
	FILE *fp = NULL;
	size_t bufsize = 100; /* initial malloc string length */

	/* Initialization of disk statistics */
	fp = fopen("./data", "r");
	if (fp == NULL) {
		endwin(); /* free memory, quit ncurses */
		fprintf(stderr, "ERROR: cannot open the file.\n");
		exit(1);
	}

	/* count number of lines in the file, minus the header */
	*nline = countlines(fp) - 1;

	iteminfo *item = (iteminfo *)malloc(*nline * sizeof(iteminfo));

	/* seek back to start of the file */
	fseek(fp, 0, SEEK_SET);

	/* Generate data structure to hold file's contents */
	char *filedata[*nline];

	for (int j = 0; j < *nline; j++)
		filedata[j] = (char *)malloc(200 * sizeof(char));

	char **ptr = filedata;
	readlines(*nline, &ptr, &bufsize, fp);

	getitemdata(item, filedata, *nline);

	/* free dynamically allocated memories used by getline call */
	for (int j = 0; j < *nline; j++)
		free(filedata[j]);
	fclose(fp);

	return item;
}

int
main(void)
{
	int numitems;
	wint_t key = 1;

	setlocale(LC_ALL, ""); /* enable long char before ncurses calls. */
	initscr(); /* initialize ncurses: memory & clear screen */
	noecho(); /* do not write keypresses to screen */
	start_color(); /* enable color support */
	initclrs(); /* initialize custom colors */
	curs_set(0); /* cursor, 0: invis, 1: blinking, 2: solid */
	ESCDELAY = 250; /* delay time in ms for esc key. */
	keypad(stdscr, TRUE); /* enable keypad to avoid escape sequences */


	winprop winfo[N_WINDOWS];
	WINDOW *win[N_WINDOWS];
	iteminfo *item[2];

	item[0] = getiteminfo(&numitems);
	item[1] = (iteminfo *)malloc(numitems * sizeof(iteminfo));

	/*                    no, height, width,             y, x, curline, start, nitem, hghtmenu */
	winfo[0] = (winprop){ 0,  LINES,  WIN_0_WIDTH,       0, 0, 0,       0,     numitems, 0 };
	winfo[1] = (winprop){ 1,  LINES,  COLS - winfo[0].w, 0, 0, 0,       0,     0,       0 };

	win[0] = newwin(winfo[0].h, winfo[0].w, 0, 0);
	win[1] = newwin(winfo[1].h, winfo[1].w, 0, winfo[0].w);
	winfo[selwin].menuh = (winfo[selwin].nitem > LINES) ? LINES : winfo[selwin].nitem;

	wbkgd(win[0], COLOR_PAIR(1)); /* set color of win[0] */
	wbkgd(win[1], COLOR_PAIR(2)); /* set color of win[1] */

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
				drwmain(win[selwin], item[selwin], &winfo[selwin]);
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
				drwmain(win[selwin], item[selwin], &winfo[selwin]);
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
				drwmain(win[selwin], item[selwin], &winfo[selwin]);
			else
				drawmenu(win[selwin], item[selwin], &winfo[selwin]);
			break;
		case '\n':
		case KEYSPACE:
			if (selwin == 0) {
				/* Only allow to increase nitems to numitems */
				if (winfo[1].nitem < numitems)
					winfo[1].nitem++;

				/* copy selected data */
				copyitem(item[0], item[1], winfo[0].curline, winfo[1].nitem - 1);

				/* Calculate required height of the menu */
				winfo[1].menuh = (winfo[1].nitem >= LINES) ? LINES : winfo[1].nitem;

				drawmenu(win[1], item[1], &winfo[1]);
			} else if (selwin == 1)
				/* TODO: on right window ask for how much grams,
				 * then calculate values accordingly also write
				 * 100gr initially */
				break;
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
