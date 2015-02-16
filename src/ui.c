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

#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include "config.h"
#include "ui.h"

/*
	Common
*/

struct tasklist_ent
{
	struct download_task *t;
	struct tasklist_ent *next;
};

struct tasklist_ent *tasks;

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
	tasks = ent;
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
nc_init()
{
	struct winsize w;
	WINDOW *tmp, *version;

	tasks = NULL;

	ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

	initscr();
	start_color();
	init_pair(1, COLOR_WHITE, COLOR_BLUE);

	version = newwin(1, w.ws_col, 0, 0);
	wattron(version, COLOR_PAIR(1));
	wbkgd(version, COLOR_PAIR(1));
	wprintw(version, " %s", PACKAGE_STRING);
	wrefresh(version);

	tmp = newwin(1, w.ws_col, w.ws_row - 2, 0);
	wattron(tmp, COLOR_PAIR(1));
	wbkgd(tmp, COLOR_PAIR(1));
	wattron(tmp, A_BOLD);
	wrefresh(tmp);

	status = newwin(1, w.ws_col - 1, w.ws_row - 2, 1);
	wattron(status, COLOR_PAIR(1));
	wbkgd(status, COLOR_PAIR(1));
	wattron(status, A_BOLD);
	wrefresh(status);

	list = newwin(w.ws_row - 3, w.ws_col, 1, 0);
	wrefresh(list);

	tmp = newwin(1, w.ws_col, w.ws_row - 1, 0);
	wprintw(tmp, "[Ctrl-C] Quit");
	wrefresh(tmp);
}

static int
nc_status(const char *fmt, ...)
{
	wclear(status);

	va_list args;
	va_start(args, fmt);
	vwprintw(status, fmt, args);
	va_end(args);

	wrefresh(status);
	return 0;
}

static int
nc_printf(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);
	vwprintw(list, fmt, args);
	va_end(args);

	wrefresh(list);
	return 0;
}

static void
nc_stop()
{
	endwin();
}

static void
nc_loop()
{
	while (1)
	{
		sleep(1);
	}
}

static void
nc_print_tasks()
{
	struct tasklist_ent *tmp;
	struct download_task *t;
	int percent;

	for (tmp = tasks; tmp != NULL; tmp = tmp->next)
	{
		t = tmp->t;
		percent = (((float) t->downloaded / t->size) * 100);

		nc_printf("* %s\n", t->fn);
		nc_printf("  %s [%d%%]", t->status, percent);

		if (!strcmp(t->status, "downloading"))
		{
			nc_printf(", ↓ %d B/s", t->speed_dn);
			nc_printf(", ↑ %d B/s", t->speed_up);
		}

		nc_printf(", ratio: %0.2f\n", (float) t->uploaded / t->size);
	}
}

/*
	Console UI
*/

static char *
cs_status_color(const char *status)
{
	if (!strcmp(status, "finished"))
		return "\033[0;32m"; /* green */
	else if (!strcmp(status, "paused"))
		return "\033[0;35m"; /* purple */
	else if (!strcmp(status, "downloading"))
		return "\033[0;36m"; /* cyan */
	else if (!strcmp(status, "waiting"))
		return "\033[0;33m"; /* yellow */
	else if (!strcmp(status, "seeding"))
		return "\033[0;34m"; /* blue */

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

		printf(", ratio: %0.2f", (float) t->uploaded / t->size);
		printf("\n");
	}
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
	ui->loop = noop;
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
