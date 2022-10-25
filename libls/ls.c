/*
  Copyright (C) 2000-2022 Free Software Foundation, Inc.

  This file is part of GNU Inetutils.

  GNU Inetutils is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or (at
  your option) any later version.

  GNU Inetutils is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see `http://www.gnu.org/licenses/'. */

/*
 * Copyright (c) 1989, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* This code is derived from software contributed to Berkeley by
   Michael Fischbein.  */

#include <config.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <errno.h>
#include <fts_.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <termios.h>

#include <inttostr.h>
#include "ls.h"
#include "extern.h"

static void display (FTSENT *, FTSENT *);
static int mastercmp (const FTSENT **, const FTSENT **);
static void traverse (int, char **, int);

static void (*printfcn) (DISPLAY *);
static int (*sortfcn) (const FTSENT *, const FTSENT *);

#define BY_NAME 0
#define BY_SIZE 1
#define BY_TIME	2

long blocksize;			/* block size units */
int termwidth = 80;		/* default terminal width */
int sortkey = BY_NAME;

static int output;		/* If anything was output. */

/* flags */
int f_accesstime;		/* use time of last access */
int f_column;			/* columnated format */
int f_columnacross;		/* columnated format, sorted across */
int f_flags;			/* show flags associated with a file */
int f_inode;			/* print inode */
int f_listdir;			/* list actual directory, not contents */
int f_listdot;			/* list files beginning with . */
int f_longform;			/* long listing format */
int f_newline;			/* if precede with newline */
int f_nonprint;			/* show unprintables as ? */
int f_nosort;			/* don't sort output */
int f_numericonly;		/* don't expand uid to symbolic name */
int f_recursive;		/* ls subdirectories also */
int f_reversesort;		/* reverse whatever sort is used */
int f_sectime;			/* print the real time for all files */
int f_singlecol;		/* use single column output */
int f_size;			/* list size in short listing */
int f_statustime;		/* use time of last mode change */
int f_stream;			/* stream format */
int f_dirname;			/* if precede with directory name */
int f_type;			/* add type character for non-regular files */
int f_typedir;			/* add type character for directories */
int f_whiteout;			/* show whiteout entries */

int rval;

int
ls_main (int argc, char **argv)
{
  static char dot[] = ".", *dotav[] = { dot, NULL };
  struct winsize win;
  int ch, fts_options;
  char *p;

  /*
   * Clear all settings made in any previous call.
   */
  output = 0;

  f_accesstime = f_column = f_columnacross = f_flags = f_inode = 0;
  f_listdir = f_listdot = f_longform = f_newline = 0;
  f_nonprint = f_nosort = f_numericonly = 0;
  f_recursive = f_reversesort = f_sectime = f_singlecol = 0;
  f_size = f_statustime = f_stream = f_dirname = 0;
  f_type = f_typedir = f_whiteout = 0;

  /* Terminal defaults to -Cq, non-terminal defaults to -1. */
  if (isatty (STDOUT_FILENO))
    {
      p = getenv ("COLUMNS");
      if (p != NULL)
	termwidth = atoi (p);
      else if (ioctl (STDOUT_FILENO, TIOCGWINSZ, &win) == 0 && win.ws_col > 0)
	termwidth = win.ws_col;
      f_column = f_nonprint = 1;
    }
  else
    f_singlecol = 1;

  /* Root is -A automatically. */
  if (!getuid ())
    f_listdot = 1;

  optind = 1;			/* Reset for reentrant scanning.  */

  fts_options = FTS_PHYSICAL | FTS_NOCHDIR;
  while ((ch = getopt (argc, argv, "1ACFLRSTWacdfgiklmnopqrstux")) != -1)
    {
      switch (ch)
	{
	  /*
	   * The options -1, -C, -l, -m, -n and -x all overrule each
	   * other so shell aliasing works right.
	   */
	case '1':
	  f_singlecol = 1;
	  f_column = f_columnacross = f_longform = f_stream = 0;
	  break;
	case 'C':
	  f_column = 1;
	  f_longform = f_columnacross = f_singlecol = f_stream = 0;
	  break;
	case 'l':
	  f_longform = 1;
	  f_numericonly = 0;
	  f_column = f_columnacross = f_singlecol = f_stream = 0;
	  break;
	case 'm':
	  f_stream = 1;
	  f_column = f_columnacross = f_longform = f_singlecol = 0;
	  break;
	case 'x':
	  f_columnacross = 1;
	  f_column = f_longform = f_singlecol = f_stream = 0;
	  break;
	case 'n':
	  f_longform = 1;
	  f_numericonly = 1;
	  f_column = f_columnacross = f_singlecol = f_stream = 0;
	  break;
	  /* The -c and -u options override each other. */
	case 'c':
	  f_statustime = 1;
	  f_accesstime = 0;
	  break;
	case 'u':
	  f_accesstime = 1;
	  f_statustime = 0;
	  break;
	case 'F':
	  f_type = 1;
	  break;
	case 'L':
	  fts_options &= ~FTS_PHYSICAL;
	  fts_options |= FTS_LOGICAL;
	  break;
	case 'R':
	  f_recursive = 1;
	  break;
	case 'a':
	  fts_options |= FTS_SEEDOT;
	case 'A':
	  f_listdot = 1;
	  break;
	  /* The -d option turns off the -R option. */
	case 'd':
	  f_listdir = 1;
	  f_recursive = 0;
	  break;
	case 'f':
	  f_nosort = 1;
	  break;
	case 'g':		/* Compatibility with 4.3BSD. */
	  break;
	case 'i':
	  f_inode = 1;
	  break;
	case 'k':
	  blocksize = 1024;
	  break;
	case 'o':
	  f_flags = 1;
	  break;
	case 'p':
	  f_typedir = 1;
	  break;
	case 'q':
	  f_nonprint = 1;
	  break;
	case 'r':
	  f_reversesort = 1;
	  break;
	case 'S':
	  sortkey = BY_SIZE;
	  break;
	case 's':
	  f_size = 1;
	  break;
	case 'T':
	  f_sectime = 1;
	  break;
	case 't':
	  sortkey = BY_TIME;
	  break;
	case 'W':
	  f_whiteout = 1;
	  break;
	default:
	  return usage ();
	}
    }
  argc -= optind;
  argv += optind;

  /*
   * If not -F, -i, -l, -p, -S, -s or -t options, don't require stat
   * information.
   */
  if (!f_longform && !f_inode && !f_size && !f_type && !f_typedir &&
      sortkey == BY_NAME)
    fts_options |= FTS_NOSTAT;

  /*
   * If not -F, -d or -l options, follow any symbolic links listed on
   * the command line.
   */
  if (!f_longform && !f_listdir && !f_type)
    fts_options |= FTS_COMFOLLOW;

  /*
   * If -W, show whiteout entries
   */
#ifdef FTS_WHITEOUT
  if (f_whiteout)
    fts_options |= FTS_WHITEOUT;
#endif

  /* If -l or -s, figure out block size. */
  if (f_longform || f_size)
    {
      blocksize = 1024;		/* Fuck this hair-splitting */
      blocksize /= 512;
    }

  /* Select a sort function. */
  if (f_reversesort)
    {
      switch (sortkey)
	{
	case BY_NAME:
	  sortfcn = revnamecmp;
	  break;
	case BY_SIZE:
	  sortfcn = revsizecmp;
	  break;
	case BY_TIME:
	  if (f_accesstime)
	    sortfcn = revacccmp;
	  else if (f_statustime)
	    sortfcn = revstatcmp;
	  else			/* Use modification time. */
	    sortfcn = revmodcmp;
	  break;
	}
    }
  else
    {
      switch (sortkey)
	{
	case BY_NAME:
	  sortfcn = namecmp;
	  break;
	case BY_SIZE:
	  sortfcn = sizecmp;
	  break;
	case BY_TIME:
	  if (f_accesstime)
	    sortfcn = acccmp;
	  else if (f_statustime)
	    sortfcn = statcmp;
	  else			/* Use modification time. */
	    sortfcn = modcmp;
	  break;
	}
    }

  /* Select a print function. */
  if (f_singlecol)
    printfcn = printscol;
  else if (f_columnacross)
    printfcn = printacol;
  else if (f_longform)
    printfcn = printlong;
  else if (f_stream)
    printfcn = printstream;
  else
    printfcn = printcol;

  if (argc)
    traverse (argc, argv, fts_options);
  else
    traverse (1, dotav, fts_options);
  return (rval);
}

/*
 * Traverse() walks the logical directory structure specified by the argv list
 * in the order specified by the mastercmp() comparison function.  During the
 * traversal it passes linked lists of structures to display() which represent
 * a superset (may be exact set) of the files to be displayed.
 */
static void
traverse (int argc, char **argv, int options)
{
  FTS *ftsp;
  FTSENT *p, *chp;
  int ch_options;

  ftsp = fts_open (argv, options, f_nosort ? NULL : mastercmp);
  if (ftsp == NULL)
    {
      fprintf (stderr, "%s: fts_open: %s", argv[0], strerror (errno));
      rval = EXIT_FAILURE;
      return;
    }

  display (NULL, fts_children (ftsp, 0));
  if (f_listdir)
    return;

  /*
   * If not recursing down this tree and don't need stat info, just get
   * the names.
   */
  ch_options = !f_recursive && options & FTS_NOSTAT ? FTS_NAMEONLY : 0;

  while ((p = fts_read (ftsp)) != NULL)
    switch (p->fts_info)
      {
      case FTS_D:
	if (p->fts_name[0] == '.' &&
	    p->fts_level != FTS_ROOTLEVEL && !f_listdot)
	  break;

	/*
	 * If already output something, put out a newline as
	 * a separator.  If multiple arguments, precede each
	 * directory with its name.
	 */
	if (output)
	  printf ("\n%s:\n", p->fts_path);
	else if (argc > 1)
	  {
	    printf ("%s:\n", p->fts_path);
	    output = 1;
	  }

	chp = fts_children (ftsp, ch_options);
	display (p, chp);

	if (!f_recursive && chp != NULL)
	  fts_set (ftsp, p, FTS_SKIP);
	break;
      case FTS_DC:
	fprintf (stderr, "%s: directory causes a cycle", p->fts_name);
	break;
      case FTS_DNR:
      case FTS_ERR:
	fprintf (stderr, "%s: %s\n", p->fts_name, strerror (p->fts_errno));
	rval = 1;
	break;
      }
  if (errno)
    {
      fprintf (stderr, "fts_read: %s", strerror (errno));
      rval = EXIT_FAILURE;
      return;
    }
}

/*
 * Display() takes a linked list of FTSENT structures and passes the list
 * along with any other necessary information to the print function.  P
 * points to the parent directory of the display list.
 */
static void
display (FTSENT * p, FTSENT * list)
{
  struct stat *sp;
  DISPLAY d;
  FTSENT *cur;
  NAMES *np;
  unsigned long long maxsize;
  unsigned long btotal, maxinode, maxlen, maxnlink;
  long maxblock;
  int bcfile, flen, glen, ulen, maxflags, maxgroup, maxuser;
  int entries, needstats;
  char *user = NULL, *group = NULL, buf[INT_BUFSIZE_BOUND (uintmax_t)];
  char nuser[INT_BUFSIZE_BOUND (uintmax_t)],
    ngroup[INT_BUFSIZE_BOUND (uintmax_t)];
  char *flags = NULL;

  /*
   * If list is NULL there are two possibilities: that the parent
   * directory p has no children, or that fts_children() returned an
   * error.  We ignore the error case since it will be replicated
   * on the next call to fts_read() on the post-order visit to the
   * directory p, and will be signalled in traverse().
   */
  if (list == NULL)
    return;

  needstats = f_inode || f_longform || f_size;
  flen = 0;
  btotal = maxblock = maxinode = maxlen = maxnlink = 0;
  bcfile = 0;
  maxuser = maxgroup = maxflags = 0;
  maxsize = 0;
  for (cur = list, entries = 0; cur; cur = cur->fts_link)
    {
      if (cur->fts_info == FTS_ERR || cur->fts_info == FTS_NS)
	{
	  fprintf (stderr, "%s: %s\n",
		   cur->fts_name, strerror (cur->fts_errno));
	  cur->fts_number = NO_PRINT;
	  rval = 1;
	  continue;
	}

      /*
       * P is NULL if list is the argv list, to which different rules
       * apply.
       */
      if (p == NULL)
	{
	  /* Directories will be displayed later. */
	  if (cur->fts_info == FTS_D && !f_listdir)
	    {
	      cur->fts_number = NO_PRINT;
	      continue;
	    }
	}
      else
	{
	  /* Only display dot file if -a/-A set. */
	  if (cur->fts_name[0] == '.' && !f_listdot)
	    {
	      cur->fts_number = NO_PRINT;
	      continue;
	    }
	}
      if (cur->fts_namelen > maxlen)
	maxlen = cur->fts_namelen;
      if (needstats)
	{
	  sp = cur->fts_statp;
	  if (sp->st_blocks > maxblock)
	    maxblock = sp->st_blocks;
	  if (sp->st_ino > maxinode)
	    maxinode = sp->st_ino;
	  if (sp->st_nlink > maxnlink)
	    maxnlink = sp->st_nlink;
	  if (sp->st_size > (off_t) maxsize)
	    maxsize = sp->st_size;

	  btotal += sp->st_blocks;
	  if (f_longform)
	    {
	      struct passwd *pwd;
	      struct group *grp;

	      user = group = NULL;

	      if (!f_numericonly)
		{
		  pwd = getpwuid (sp->st_uid);
		  if (pwd)
		    user = pwd->pw_name;
		  grp = getgrgid (sp->st_gid);
		  if (grp)
		    group = grp->gr_name;
		}
	      if (!user)
		user = umaxtostr (sp->st_uid, nuser);
	      if (!group)
		group = umaxtostr (sp->st_gid, ngroup);

	      ulen = strlen (user);
	      if (ulen > maxuser)
		maxuser = ulen;
	      glen = strlen (group);
	      if (glen > maxgroup)
		maxgroup = glen;

	      if (f_flags)
		{
		  flags = "-";
		  flen = strlen (flags);
		  if (flen > maxflags)
		    maxflags = flen;
		}
	      else
		flen = 0;

	      np = malloc (sizeof (NAMES) + ulen + glen + flen + 3);
	      if (np == NULL)
		{
		  fprintf (stderr, "malloc: %s", strerror (errno));
		  rval = EXIT_FAILURE;
		  return;
		}

	      np->user = &np->data[0];
	      strcpy (np->user, user);
	      np->group = &np->data[ulen + 1];
	      strcpy (np->group, group);

	      if (S_ISCHR (sp->st_mode) || S_ISBLK (sp->st_mode))
		bcfile = 1;

	      if (f_flags)
		{
		  np->flags = &np->data[ulen + glen + 2];
		  strcpy (np->flags, flags);
		}
	      cur->fts_pointer = np;
	    }
	}
      ++entries;
    }

  if (!entries)
    return;

  d.list = list;
  d.entries = entries;
  d.maxlen = maxlen;
  if (needstats)
    {
      d.bcfile = bcfile;
      d.btotal = btotal;
      d.s_block = strlen (umaxtostr (maxblock, buf));
      d.s_flags = maxflags;
      d.s_group = maxgroup;
      d.s_inode = strlen (umaxtostr (maxinode, buf));
      d.s_nlink = strlen (umaxtostr (maxnlink, buf));
      d.s_size = strlen (umaxtostr (maxsize, buf));
      d.s_user = maxuser;
    }

  printfcn (&d);
  output = 1;

  if (f_longform)
    for (cur = list; cur; cur = cur->fts_link)
      free (cur->fts_pointer);
}

/*
 * Ordering for mastercmp:
 * If ordering the argv (fts_level = FTS_ROOTLEVEL) return non-directories
 * as larger than directories.  Within either group, use the sort function.
 * All other levels use the sort function.  Error entries remain unsorted.
 */
static int
mastercmp (const FTSENT ** a, const FTSENT ** b)
{
  int a_info, b_info;

  a_info = (*a)->fts_info;
  if (a_info == FTS_ERR)
    return (0);
  b_info = (*b)->fts_info;
  if (b_info == FTS_ERR)
    return (0);

  if (a_info == FTS_NS || b_info == FTS_NS)
    {
      if (b_info != FTS_NS)
	return (1);
      else if (a_info != FTS_NS)
	return (-1);
      else
	return (namecmp (*a, *b));
    }

  if (a_info != b_info && (*a)->fts_level == FTS_ROOTLEVEL && !f_listdir)
    {
      if (a_info == FTS_D)
	return (1);
      if (b_info == FTS_D)
	return (-1);
    }
  return (sortfcn (*a, *b));
}
