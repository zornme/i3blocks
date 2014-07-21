/*
 * block.c - update of a single status line block
 * Copyright (C) 2014  Vivien Didelot
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "block.h"
#include "log.h"

static void
child_setup_env(struct block *block)
{
	if (setenv("BLOCK_NAME", block->name, 1) == -1)
		_exit(1);

	if (setenv("BLOCK_INSTANCE", block->instance, 1) == -1)
		_exit(1);

	if (setenv("BLOCK_BUTTON", block->click.button, 1) == -1)
		_exit(1);

	if (setenv("BLOCK_X", block->click.x, 1) == -1)
		_exit(1);

	if (setenv("BLOCK_Y", block->click.y, 1) == -1)
		_exit(1);
}

static void
child_reset_signals(void)
{
	sigset_t set;

	if (sigfillset(&set) == -1)
		_exit(1);

	/* It should be safe to assume that all signals are unblocked by default */
	if (sigprocmask(SIG_UNBLOCK, &set, NULL) == -1)
		_exit(1);
}

static void
child_redirect_stdout(int pipe[2])
{
	if (close(pipe[0]) == -1)
		_exit(1);

	/* Defensive check */
	if (pipe[1] == STDOUT_FILENO)
		return;

	if (dup2(pipe[1], STDOUT_FILENO) == -1)
		_exit(1);

	if (close(pipe[1]) == -1)
		_exit(1);
}

static void
linecpy(char **lines, char *dest, unsigned int size)
{
	char *newline = strchr(*lines, '\n');

	/* split if there's a newline */
	if (newline)
		*newline = '\0';

	/* if text in non-empty, copy it */
	if (**lines) {
		strncpy(dest, *lines, size);
		*lines += strlen(dest);
	}

	/* increment if next char is non-null */
	if (*(*lines + 1))
		*lines += 1;
}

static void
mark_as_failed(struct block *block, const char *reason, int status)
{
	static const size_t short_size = sizeof(block->short_text);
	static const size_t full_size = sizeof(block->full_text);
	char short_text[short_size];
	char full_text[full_size];

	if (status < 0)
		snprintf(short_text, short_size, "[%s] ERROR", block->name);
	else
		snprintf(short_text, short_size, "[%s] ERROR (exit:%d)", block->name, status);

	if (*reason)
		snprintf(full_text, full_size, "%s %s", short_text, reason);
	else
		snprintf(full_text, full_size, "%s", short_text);

	strncpy(block->full_text, full_text, full_size);
	strncpy(block->min_width, full_text, sizeof(block->min_width) - 1);
	strncpy(block->short_text, short_text, short_size);
	strcpy(block->color, "#FF0000");
	strcpy(block->urgent, "true");
}

void
block_spawn(struct block *block)
{
	int out[2];

	if (!*block->command) {
		bdebug(block, "no command, skipping");
		return;
	}

	if (block->pid > 0) {
		bdebug(block, "process already spawned");
		return;
	}

	if (pipe(out) == -1) {
		berrorx(block, "pipe");
		return mark_as_failed(block, "failed to pipe", -1);
	}

	block->pid = fork();
	if (block->pid == -1) {
		berrorx(block, "fork");
		return mark_as_failed(block, "failed to fork", -1);
	}

	/* Child? */
	if (block->pid == 0) {
		child_setup_env(block);
		child_reset_signals();
		child_redirect_stdout(out);
		execl("/bin/sh", "/bin/sh", "-c", block->command, (char *) NULL);
		_exit(1); /* Unlikely */
	}

	/* Parent */
	if (close(out[1]) == -1)
		berrorx(block, "close stdout write end");
	block->pipe = out[0];
	block->timestamp = time(NULL);
	bdebug(block, "forked child %d at %ld", block->pid, block->timestamp);
}

void
block_update(struct block *block)
{
	char output[2048] = { 0 };
	char *text = output;

	/*
	 * FIXME nonblock necessary since the pipe should be closed?
	 *       what about cloexec?
	 */

	/* Note read(2) returns 0 for end-of-pipe */
	if (read(block->pipe, output, sizeof(output)) == -1) {
		berrorx(block, "read");
		return mark_as_failed(block, "failed to read pipe", -1);
	}

	if (close(block->pipe) == -1) {
		berror(block, "failed to close");
		return mark_as_failed(block, "failed to close read pipe", -1);
	}

	if (block->code != 0 && block->code != '!') {
		char reason[1024] = { 0 };

		berror(block, "bad exit code %d", block->code);
		linecpy(&text, reason, sizeof(reason) - 1);
		return mark_as_failed(block, reason, block->code);
	}

	/* From here, the update went ok so merge the output */
	strncpy(block->urgent, block->code == '!' ? "true" : "false", sizeof(block->urgent) - 1);
	linecpy(&text, block->full_text, sizeof(block->full_text) - 1);
	linecpy(&text, block->short_text, sizeof(block->short_text) - 1);
	linecpy(&text, block->color, sizeof(block->color) - 1);
	bdebug(block, "updated successfully");
}
