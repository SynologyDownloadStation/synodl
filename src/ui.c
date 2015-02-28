/*

SynoDL - CLI for Synology's DownloadStation
Copyright (C) 2015  Stefan Ott

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.

*/

#include <math.h>
#include <ncurses.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include "config.h"
#include "syno.h"
#include "ui.h"

/*
	Common
*/

struct tasklist_ent
{
	struct task *t;
	struct tasklist_ent *next;
	struct tasklist_ent *prev;
};

struct tasklist_ent *tasks;
struct tasklist_ent *nc_selected_task;

static void
add_task(struct task *t)
{
	struct tasklist_ent *ent = malloc(sizeof(struct tasklist_ent));

	if (!ent)
	{
		fprintf(stderr, "Malloc failed\n");
		return;
	}

	ent->t = malloc(sizeof(struct task));

	if (!ent->t)
	{
		fprintf(stderr, "Malloc failed\n");
		return;
	}

	memcpy(ent->t, t, sizeof(struct task));

	ent->next = tasks;
	ent->prev = NULL;

	if (tasks)
	{
		tasks->prev = ent;
	}

	tasks = ent;

	nc_selected_task = ent;
}

static void
free_tasks()
{
	struct tasklist_ent *ent, *tmp;

	ent = tasks;

	while (ent)
	{
		tmp = ent;
		ent = ent->next;

		free(tmp->t);
		free(tmp);
	}

	tasks = NULL;
}

static void
noop()
{
}

/*
	Curses UI
*/

static WINDOW *status, *list, *version;

static void
nc_status_color(const char *status, WINDOW *win)
{
	if (!strcmp(status, "waiting"))
		wattron(win, COLOR_PAIR(7)); /* yellow */
	else if (!strcmp(status, "downloading"))
		wattron(win, COLOR_PAIR(6)); /* cyan */
	else if (!strcmp(status, "paused"))
		wattron(win, COLOR_PAIR(8)); /* magenta */
	else if (!strcmp(status, "finishing"))
		wattron(win, COLOR_PAIR(6)); /* cyan */
	else if (!strcmp(status, "finished"))
		wattron(win, COLOR_PAIR(3)); /* green */
	else if (!strcmp(status, "hash_checking"))
		wattron(win, COLOR_PAIR(6)); /* cyan */
	else if (!strcmp(status, "seeding"))
		wattron(win, COLOR_PAIR(4)); /* blue */
	else if (!strcmp(status, "filehosting_waiting"))
		wattron(win, COLOR_PAIR(7)); /* yellow */
	else if (!strcmp(status, "extracting"))
		wattron(win, COLOR_PAIR(6)); /* cyan */
	else /* error */
		wattron(win, COLOR_PAIR(5)); /* red */
}

static void
nc_status_color_off(const char *status, WINDOW *win)
{
	if (!strcmp(status, "waiting"))
		wattron(win, COLOR_PAIR(7)); /* yellow */
	else if (!strcmp(status, "downloading"))
		wattroff(win, COLOR_PAIR(6)); /* cyan */
	else if (!strcmp(status, "paused"))
		wattroff(win, COLOR_PAIR(8)); /* magenta */
	else if (!strcmp(status, "finishing"))
		wattroff(win, COLOR_PAIR(6)); /* cyan */
	else if (!strcmp(status, "finished"))
		wattroff(win, COLOR_PAIR(3)); /* green */
	else if (!strcmp(status, "hash_checking"))
		wattroff(win, COLOR_PAIR(6)); /* cyan */
	else if (!strcmp(status, "seeding"))
		wattroff(win, COLOR_PAIR(4)); /* blue */
	else if (!strcmp(status, "filehosting_waiting"))
		wattron(win, COLOR_PAIR(7)); /* yellow */
	else if (!strcmp(status, "extracting"))
		wattroff(win, COLOR_PAIR(6)); /* cyan */
	else /* error */
		wattroff(win, COLOR_PAIR(5)); /* red */
}

static void
unit(double size, char *buf, ssize_t len)
{
	char u[] = "BkMGTPEZY";
	int cur = 0;


	while ((size > 1024) && (cur < strlen(u)))
	{
		cur += 1;
		size /= 1024;
	}

	if (size < 10)
	{
		snprintf(buf, len, "%1.1f%c", roundf(size * 100) / 100, u[cur]);
	}
	else
	{
		snprintf(buf, len, "%ld%c", lround(size), u[cur]);
	}
}

static int
nc_status(const char *fmt, ...)
{
	char buf[6];
	time_t now;

	now = time(NULL);
	strftime(buf, sizeof(buf), "%H:%M", localtime(&now));

	mvwhline(status, 0, 1, ' ', COLS);
	mvwprintw(status, 0, 1, "[%s]", buf);

	va_list args;
	va_start(args, fmt);
	mvwprintw(status, 0, 10, fmt, args);
	va_end(args);

	wrefresh(status);
	return 0;
}

static int
nc_status_totals(int up, int dn)
{
	char up_buf[32], dn_buf[32];
	char speed[128];

	unit(up, up_buf, sizeof(up_buf));
	unit(dn, dn_buf, sizeof(dn_buf));
	snprintf(speed, sizeof(speed), "↑ %s/s, ↓ %s/s.  Press '?' for help.",
								up_buf, dn_buf);
	return nc_status(speed);
}

static void
nc_print_tasks()
{
	struct tasklist_ent *tmp;
	struct task *t;
	int i, tn_width, total_dn, total_up;
	char fmt[16];
	char buf[32];

	i = 0;
	total_dn = 0;
	total_up = 0;

	tn_width = COLS - 24;
	snprintf(fmt, sizeof(fmt), "%%-%d.%ds", tn_width, tn_width);

	wattron(list, COLOR_PAIR(10));
	mvwprintw(list, i, 0, " ");
	mvwprintw(list, i, 1, fmt, "Download task");
	mvwprintw(list, i, tn_width + 2, "Size");
	mvwprintw(list, i, tn_width + 7, "%-11.11s Prog ", "Status");
	mvwhline(list, i, tn_width + 1, ACS_VLINE, 1);
	mvwhline(list, i, tn_width + 6, ACS_VLINE, 1);
	mvwhline(list, i, tn_width + 18, ACS_VLINE, 1);
	wattroff(list, COLOR_PAIR(10));

	i += 1;

	for (tmp = tasks; tmp != NULL; tmp = tmp->next)
	{
		t = tmp->t;

		total_dn += t->speed_dn;
		total_up += t->speed_up;

		wmove(list, i, 0);
		wclrtoeol(list);

		if (tmp == nc_selected_task)
		{
			wattron(list, A_BOLD);
			mvwprintw(list, i, 0, ">");
			mvwprintw(list, i, COLS - 1, "<");
		}

		/* file name */
		mvwprintw(list, i, 1, fmt, t->fn);

		/* size */
		unit(t->size, buf, sizeof(buf));
		mvwprintw(list, i, tn_width + 2, "%-5s", buf);

		/* status */
		nc_status_color(t->status, list);
		mvwprintw(list, i, tn_width + 7, "%-11s", t->status);
		nc_status_color_off(t->status, list);

		/* percent */
		mvwprintw(list, i, tn_width + 19, "%3d%%", t->percent_dn);

		wattroff(list, COLOR_PAIR(2));
		wattroff(list, A_BOLD);

		mvwhline(list, i, tn_width + 1, ACS_VLINE, 1);
		mvwhline(list, i, tn_width + 6, ACS_VLINE, 1);
		mvwhline(list, i, tn_width + 18, ACS_VLINE, 1);

		i++;
	}

	nc_status_totals(total_up, total_dn);

	wmove(list, i, 0);
	wclrtobot(list);
	wrefresh(list);
}


static void
nc_alert(const char *text)
{
	WINDOW *win, *ok;
	int width;

	width = strlen(text) + 4;

	win = newwin(5, width, (LINES / 2) - 3, ((COLS - width) / 2));
	wattron(win, COLOR_PAIR(11));
	wbkgd(win, COLOR_PAIR(11));
	box(win, 0, 0);

	mvwprintw(win, 1, 2, "%s", text);

	wattroff(win, COLOR_PAIR(2));
	wrefresh(win);

	nc_status("Alert");

	ok = derwin(win, 1, 6, 3, (width / 2 ) - 3);
	wprintw(ok, "[ Ok ]");
	wbkgd(ok, COLOR_PAIR(10));

	wrefresh(ok);

	keypad(win, TRUE);

	while (wgetch(win) != 10) {}

	delwin(ok);
	delwin(win);

	touchwin(list);
	nc_print_tasks();
}

static void
nc_select_first()
{
	if (!nc_selected_task)
	{
		return;
	}

	while (nc_selected_task->prev)
	{
		nc_selected_task = nc_selected_task->prev;
	}
}

static void
nc_select_last()
{
	if (!nc_selected_task)
	{
		return;
	}

	while (nc_selected_task->next)
	{
		nc_selected_task = nc_selected_task->next;
	}
}

static void
nc_select_prev()
{
	if (!nc_selected_task)
	{
		return;
	}

	if (nc_selected_task->prev)
	{
		nc_selected_task = nc_selected_task->prev;
	}
}

static void
nc_select_next()
{
	if (!nc_selected_task)
	{
		return;
	}

	if (nc_selected_task->next)
	{
		nc_selected_task = nc_selected_task->next;
	}
}

static void
nc_status_bar()
{
	if (status)
	{
		delwin(status);
	}

	status = newwin(1, COLS, LINES - 1, 0);
	wattron(status, COLOR_PAIR(1));
	wbkgd(status, COLOR_PAIR(1));
	wrefresh(status);

	keypad(status, TRUE);
}

static void
nc_task_window()
{
	if (list)
	{
		delwin(list);
	}

	list = newwin(LINES - 1, COLS, 0, 0);
	wrefresh(list);
}

void handle_winch(int sig)
{
	endwin();
	refresh();
	clear();

	nc_status_bar();
	nc_task_window();
	nc_print_tasks();

	wrefresh(status);
}

static void
nc_init()
{
	tasks = NULL;
	nc_selected_task = NULL;

	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = handle_winch;
	sigaction(SIGWINCH, &sa, NULL);

	cbreak();
	initscr();
	noecho();
	curs_set(0);

	start_color();
	init_pair(1, COLOR_WHITE, COLOR_BLUE);
	init_pair(2, COLOR_BLACK, COLOR_GREEN);
	init_pair(3, COLOR_GREEN, COLOR_BLACK);
	init_pair(4, COLOR_BLUE, COLOR_BLACK);
	init_pair(5, COLOR_RED, COLOR_BLACK);
	init_pair(6, COLOR_CYAN, COLOR_BLACK);
	init_pair(7, COLOR_YELLOW, COLOR_BLACK);
	init_pair(8, COLOR_MAGENTA, COLOR_BLACK);
	init_pair(9, COLOR_BLACK, COLOR_CYAN);
	init_pair(10, COLOR_BLACK, COLOR_WHITE);
	init_pair(11, COLOR_WHITE, COLOR_RED);

	nc_status_bar();
	nc_task_window();
}

static void
nc_stop()
{
	delwin(version);
	delwin(status);
	endwin();
}

static void
nc_help()
{
	WINDOW *win, *help;

	win = newwin(8, 32, (LINES / 2) - 4, (COLS / 2) - 16);
	wattron(win, COLOR_PAIR(1));
	wbkgd(win, COLOR_PAIR(1));
	box(win, 0, 0);
	mvwprintw(win, 0, 5, "[ Keyboard shortcuts ]");
	wattroff(win, COLOR_PAIR(2));
	wrefresh(win);

	help = derwin(win, 4, 28, 2, 3);
	wattron(help, A_BOLD);
	wprintw(help, "A");
	wattroff(help, A_BOLD);
	wprintw(help, " ... Add download task\n");
	wattron(help, A_BOLD);
	wprintw(help, "D");
	wattroff(help, A_BOLD);
	wprintw(help, " ... Delete selected task\n");
	wattron(help, A_BOLD);
	wprintw(help, "Q");
	wattroff(help, A_BOLD);
	wprintw(help, " ... Quit\n");
	wattron(help, A_BOLD);
	wprintw(help, "R");
	wattroff(help, A_BOLD);
	wprintw(help, " ... Refresh list\n");

	touchwin(win);
	wrefresh(help);

	wgetch(help);

	delwin(help);
	delwin(win);

	touchwin(list);
	nc_print_tasks();
}

static void
nc_add_task(struct syno_ui *ui, const char *base, struct session *s)
{
	WINDOW *win, *prompt;
	char str[1024];

	win = newwin(4, COLS - 4, (LINES / 2) - 3, 2);
	wattron(win, COLOR_PAIR(1));
	wbkgd(win, COLOR_PAIR(1));
	box(win, 0, 0);
	mvwprintw(win, 0, 3, "[ Add download task ]");
	mvwprintw(win, 1, 2, "Enter URL:");
	wattroff(win, COLOR_PAIR(2));
	wrefresh(win);

	prompt = derwin(win, 1, COLS - 8, 2, 2);
	wbkgd(prompt, COLOR_PAIR(9));

	touchwin(win);
	wrefresh(prompt);

	echo();
	curs_set(1);
	wgetnstr(prompt, str, sizeof(str) - 1);
	curs_set(0);
	noecho();

	delwin(prompt);
	delwin(win);

	if (strcmp(str, "") != 0)
	{
		if (syno_download(base, s, str) != 0)
		{
			nc_alert("Failed to add task");
		}
		else
		{
			nc_status("Download task added");
		}
	}

	touchwin(list);
	nc_print_tasks();
}

static void
nc_delete_task(struct syno_ui *ui, const char *base, struct session *s)
{
	WINDOW *win, *yes, *no;
	char buf[16];
	int ok, key;

	if (!nc_selected_task)
	{
		return;
	}

	win = newwin(5, 21, (LINES / 2) - 3, (COLS / 2) - 10);
	wattron(win, COLOR_PAIR(11));
	wbkgd(win, COLOR_PAIR(11));
	box(win, 0, 0);
	mvwprintw(win, 0, 5, "[ Delete ]");
	mvwprintw(win, 1, 2, "Delete this task?");
	wattroff(win, COLOR_PAIR(2));
	wrefresh(win);

	no = derwin(win, 1, 6, 3, 3);
	wprintw(no, "[ No ]");

	yes = derwin(win, 1, 7, 3, 11);
	wprintw(yes, "[ Yes ]");

	touchwin(win);
	wrefresh(no);
	wrefresh(yes);

	wbkgd(no, COLOR_PAIR(10));
	keypad(win, TRUE);

	ok = 0;

	while ((key = wgetch(win)) != 10)
	{
		switch (key)
		{
		case 9: /* TAB */
			ok = 1 - ok;
			break;
		case KEY_LEFT:
			ok = 0;
			break;
		case KEY_RIGHT:
			ok = 1;
			break;
		case 0x59: /* Y */
		case 0x79: /* y */
			ok = 1;
			break;
		case 0x4e: /* N */
		case 0x6e: /* n */
			ok = 0;
			break;
		}

		if (ok)
		{
			wbkgd(no, COLOR_PAIR(11));
			wbkgd(yes, COLOR_PAIR(10));
		}
		else
		{
			wbkgd(no, COLOR_PAIR(10));
			wbkgd(yes, COLOR_PAIR(11));
		}
		wrefresh(no);
		wrefresh(yes);
	}

	delwin(no);
	delwin(yes);
	delwin(win);

	if (ok)
	{
		snprintf(buf, sizeof(buf), "%s", nc_selected_task->t->id);

		if (syno_delete(base, s, buf) != 0)
		{
			nc_alert("Failed to delete task");
		}
		else
		{
			nc_status("Download task added");
		}
	}

	touchwin(list);
	nc_print_tasks();
}

static void
nc_loop(struct syno_ui *ui, const char *base, struct session *s)
{
	int key;

	while ((key = wgetch(status)) != 27)
	{
		switch (key)
		{
		case KEY_UP:
			nc_select_prev();
			nc_print_tasks();
			break;
		case KEY_DOWN:
			nc_select_next();
			nc_print_tasks();
			break;
		case KEY_HOME:
			nc_select_first();
			nc_print_tasks();
			break;
		case KEY_END:
			nc_select_last();
			nc_print_tasks();
			break;
		case 0x3f: /* ? */
			nc_help();
			break;
		case 97:  /* a */
		case 65:  /* A */
			nc_status("Adding task...");
			nc_add_task(ui, base, s);
			break;
		case 100: /* d */
		case 68:  /* D */
			nc_status("Deleting task...");
			nc_delete_task(ui, base, s);
			break;
		case 113: /* q */
		case 81:  /* Q */
			nc_status("Terminating...");
			return;
		case 114: /* r */
		case 82:  /* R */
			nc_status("Refreshing...");
			free_tasks();
			if (syno_list(base, s, add_task) != 0)
			{
				nc_alert("Could not refresh data");
			}
			else
			{
				nc_print_tasks();
			}
			break;
		default:
			break;
		}
	}
}

/*
	Console UI
*/

static char *
cs_status_color(const char *status)
{
	if (!strcmp(status, "waiting"))
		return "\033[0;33m"; /* yellow */
	else if (!strcmp(status, "downloading"))
		return "\033[0;36m"; /* cyan */
	else if (!strcmp(status, "paused"))
		return "\033[0;35m"; /* magenta */
	else if (!strcmp(status, "finishing"))
		return "\033[0;36m"; /* cyan */
	else if (!strcmp(status, "finished"))
		return "\033[0;32m"; /* green */
	else if (!strcmp(status, "hash_checking"))
		return "\033[0;36m"; /* cyan */
	else if (!strcmp(status, "seeding"))
		return "\033[0;34m"; /* blue */
	else if (!strcmp(status, "filehosting_waiting"))
		return "\033[0;33m"; /* yellow */
	else if (!strcmp(status, "extracting"))
		return "\033[0;36m"; /* cyan */
	else /* error */
		return "\033[0;31m"; /* red */
}

static char *
cs_reset_color()
{
	return "\033[0m";
}

static int
cs_status(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);

	printf("\n");

	return 0;
}

static void
cs_print_tasks()
{
	struct tasklist_ent *tmp;
	struct task *t;

	for (tmp = tasks; tmp != NULL; tmp = tmp->next)
	{
		t = tmp->t;
		printf("* [%s] %s\n", t->id, t->fn);
		printf("  %s%s%s", cs_status_color(t->status), t->status,
							cs_reset_color());
		printf(" [%d%%]", (int)(((float) t->downloaded / t->size)*100));

		if (!strcmp(t->status, "downloading"))
		{
			printf(", ↓ %d B/s", t->speed_dn);
			printf(", ↑ %d B/s", t->speed_up);
		}

		printf(", ratio: %0.2f\n", (float) t->uploaded / t->size);
	}
}

static void
no_loop(struct syno_ui *ui, const char *base, struct session *s)
{
}

/*
	Initialization
*/

void
console_ui(struct syno_ui *ui)
{
	ui->init = noop;
	ui->status = cs_status;
	ui->stop = noop;
	ui->loop = no_loop;
	ui->add_task = add_task;
	ui->free = free_tasks;
	ui->render = cs_print_tasks;
}

void
curses_ui(struct syno_ui *ui)
{
	ui->init = nc_init;
	ui->status = nc_status;
	ui->stop = nc_stop;
	ui->loop = nc_loop;
	ui->add_task = add_task;
	ui->free = free_tasks;
	ui->render = nc_print_tasks;
}
