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
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>

#include "config.h"
#include "ui.h"

WINDOW *status, *list;

static void
ui_init()
{
	struct winsize w;
	WINDOW *tmp, *version;

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
ui_status(const char *fmt, ...)
{
	wclear(status);

	va_list argList;
	va_start(argList, fmt);
	vwprintw(status, fmt, argList);
	va_end(argList);

	wrefresh(status);
	return 0;
}

static int
ui_printf(const char *fmt, ...)
{
	va_list argList;
	va_start(argList, fmt);
	vwprintw(list, fmt, argList);
	va_end(argList);

	wrefresh(list);
	return 0;
}

static void
ui_stop()
{
	endwin();
}

static void
ui_loop()
{
	while (1)
	{
		sleep(1);
	}
}

static void
nothing()
{
}

void console_ui(struct syno_ui *ui)
{
	ui->init = nothing;
	ui->status = printf;
	ui->printf = printf;
	ui->stop = nothing;
	ui->loop = nothing;
}

void curses_ui(struct syno_ui *ui)
{
	ui->init = ui_init;
	ui->status = ui_status;
	ui->printf = ui_printf;
	ui->stop = ui_stop;
	ui->loop = ui_loop;
}
