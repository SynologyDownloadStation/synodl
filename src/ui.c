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
	struct download_task *t;
	struct tasklist_ent *next;
	struct tasklist_ent *prev;
};

struct tasklist_ent *tasks;
struct tasklist_ent *nc_selected_task;

static void
add_task(struct download_task *t)
{
	struct tasklist_ent *ent = malloc(sizeof(struct tasklist_ent));

	if (!ent)
	{
		fprintf(stderr, "Malloc failed\n");
		return;
	}

	ent->t = malloc(sizeof(struct download_task));

	if (!ent->t)
	{
		fprintf(stderr, "Malloc failed\n");
		return;
	}

	memcpy(ent->t, t, sizeof(struct download_task));

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

WINDOW *status, *list;

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
		wattron(win, COLOR_PAIR(8)); /* magenta */
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

static void
nc_print_tasks()
{
	struct tasklist_ent *tmp;
	struct download_task *t;
	int percent, i, tn_width;
	char speed[80];
	char fmt[16];
	char buf[32];

	i = 0;

	tn_width = COLS - 24;
	snprintf(fmt, sizeof(fmt), "%%-%d.%ds", tn_width, tn_width);

	mvwprintw(list, i, 1, fmt, "Task");
	mvwprintw(list, i, tn_width + 2, "Size");
	mvwprintw(list, i, tn_width + 7, "%-11.11s Prog", "Status");
	mvwhline(list, i, tn_width + 1, ACS_VLINE, 1);
	mvwhline(list, i, tn_width + 6, ACS_VLINE, 1);
	mvwhline(list, i, tn_width + 18, ACS_VLINE, 1);

	i += 1;

	for (tmp = tasks; tmp != NULL; tmp = tmp->next)
	{
		t = tmp->t;
		memset(speed, 0, sizeof(speed));

		wmove(list, 0, i);

		if ((t->size == 0) || (t->downloaded == 0))
		{
			percent = 0;
		}
		else
		{
			percent = (((float) t->downloaded / t->size) * 100);
		}

		wattron(list, A_BOLD);
		if (tmp == nc_selected_task)
		{
			wattron(list, COLOR_PAIR(2));
		}

		mvwhline(list, i, 0, ' ', COLS);
		mvwprintw(list, i, 1, fmt, t->fn);
		wattroff(list, A_BOLD);

		unit(t->size, buf, sizeof(buf));
		mvwprintw(list, i, tn_width + 2, "%-5s", buf);

		if (tmp != nc_selected_task)
		{
			nc_status_color(t->status, list);
		}
		mvwprintw(list, i, tn_width + 7, "%-11s", t->status);
		if (tmp != nc_selected_task)
		{
			nc_status_color_off(t->status, list);
		}
		mvwprintw(list, i, tn_width + 19, "%03d%%", percent);

		if (tmp != nc_selected_task)
		{
			wattroff(list, A_BOLD);
		}
		if (!strcmp(t->status, "downloading"))
		{
			snprintf(speed, sizeof(speed), ", D: %d B/s, U: %d B/s",
						t->speed_dn, t->speed_up);
		}

		wattroff(list, COLOR_PAIR(2));

		mvwhline(list, i, tn_width + 1, ACS_VLINE, 1);
		mvwhline(list, i, tn_width + 6, ACS_VLINE, 1);
		mvwhline(list, i, tn_width + 18, ACS_VLINE, 1);

		i++;
	}
	wrefresh(list);
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
	WINDOW *tmp;

	tmp = newwin(1, COLS, LINES - 2, 0);
	wattron(tmp, COLOR_PAIR(1));
	wbkgd(tmp, COLOR_PAIR(1));
	wattron(tmp, A_BOLD);
	wrefresh(tmp);

	status = newwin(1, COLS, LINES - 2, 1);
	wattron(status, COLOR_PAIR(1));
	wbkgd(status, COLOR_PAIR(1));
	wattron(status, A_BOLD);
	wrefresh(status);

	keypad(status, TRUE);
}

static void
nc_task_window()
{
	list = newwin(LINES - 3, COLS, 1, 0);
	wrefresh(list);
}

static void
nc_help_bar()
{
	WINDOW *tmp;

	tmp = newwin(1, COLS, LINES - 1, 0);
	wprintw(tmp, "Q)uit  R)efresh  A)dd task");
	wrefresh(tmp);
}

void handle_winch(int sig)
{
	endwin();
	refresh();
	clear();

	/* TODO: copy status */
	nc_status_bar();
	nc_task_window();
	nc_print_tasks();
	nc_help_bar();

	wprintw(status, "%s", "WINCH");
	wrefresh(status);
}

static void
nc_init()
{
	struct winsize w;
	WINDOW *version;

	cbreak();
	noecho();

	tasks = NULL;

	// TODO: check and apply
	// http://stackoverflow.com/questions/13707137/ncurses-resizing-glitch
	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

	struct sigaction sa;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_handler = handle_winch;
	sigaction(SIGWINCH, &sa, NULL);

	initscr();
	start_color();
	init_pair(1, COLOR_BLACK, COLOR_WHITE);
	init_pair(2, COLOR_WHITE, COLOR_BLUE);
	init_pair(3, COLOR_GREEN, COLOR_BLACK);
	init_pair(4, COLOR_BLUE, COLOR_BLACK);
	init_pair(5, COLOR_RED, COLOR_BLACK);
	init_pair(6, COLOR_CYAN, COLOR_BLACK);
	init_pair(7, COLOR_YELLOW, COLOR_BLACK);
	init_pair(8, COLOR_MAGENTA, COLOR_BLACK);

	version = newwin(1, w.ws_col, 0, 0);
	wattron(version, COLOR_PAIR(1));
	wbkgd(version, COLOR_PAIR(1));
	wprintw(version, " %s", PACKAGE_STRING);
	wrefresh(version);

	nc_status_bar();
	nc_task_window();
	nc_help_bar();

	nc_selected_task = NULL;
}

static int
nc_status(const char *fmt, ...)
{
	mvwhline(status, 0, 0, ' ', COLS);

	va_list args;
	va_start(args, fmt);
	mvwprintw(status, 0, 0, fmt, args);
	va_end(args);

	wrefresh(status);
	return 0;
}

static void
nc_stop()
{
	endwin();
}

static void
nc_add_task(struct syno_ui *ui, const char *base, struct session *s)
{
	WINDOW *prompt;
	char str[1024];

	prompt = newwin(5, COLS - 4, (LINES / 2) - 1, 2);
	wattron(prompt, COLOR_PAIR(2));
	box(prompt, 0, 0);
	mvwprintw(prompt, 0, 3, "[ Add task: Enter URL ]");
	wattroff(prompt, COLOR_PAIR(2));
	wrefresh(prompt);

	mvwgetnstr(prompt, 1, 3, str, sizeof(str) - 1);

	delwin(prompt);

	if (strcmp(str, "") == 0)
	{
		nc_status("Aborted");
	}
	else
	{
		syno_download(ui, base, s, str);
		nc_status("Download task added");
	}
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
		case 65: /* A */
			nc_add_task(ui, base, s);
			break;
		case 81: /* Q */
			nc_status("Terminating...");
			return;
		case 82: /* R */
			nc_status("Refreshing...");
			syno_info(ui, base, s);
			nc_print_tasks();
			nc_status("All done");
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
	struct download_task *t;

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
