/*
 *      rsvndump - remote svn repository dump
 *      Copyright (C) 2008-2011 Jonas Gehring
 *
 *      This program is free software: you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation, either version 3 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *      file: logger.h
 *      desc: Simple logging to stderr
 */


#ifndef LOGGER_H_
#define LOGGER_H_


/* Global variable: verbosity level */
extern int loglevel;

extern int L0(const char *format, ...);
extern int L1(const char *format, ...);
extern int L2(const char *format, ...);

#ifndef DEBUG
 // Fix inline for MSVC
 #if defined(_MSC_VER) && !defined(__cplusplus)
  #define inline __inline
 #endif
extern inline int LDEBUG(const char *format, ...) { (void)format; return 0; }
#else
extern int LDEBUG(const char *format, ...);
#endif
#define DEBUG_MSG LDEBUG


#endif
