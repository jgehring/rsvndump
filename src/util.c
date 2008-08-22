/*
 *	rsvndump - remote svn repository dump
 *	Copyright (C) 2008 Jonas Gehring
 *
 *	This program is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License along
 *	with this program; if not, write to the Free Software Foundation, Inc.,
 *	51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */


#include "util.h"


// Returns an entry with default fields
change_entry_t default_entry()
{
	change_entry_t entry;
	entry.path = entry.localpath = NULL;
	entry.kind = NK_NONE;
	entry.action = -1;
	entry.copy_from_path = NULL;
	entry.copy_from_rev = -2;
	entry.use_copy = -1;
	return entry;
}
