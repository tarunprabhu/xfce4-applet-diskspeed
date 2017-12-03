/*
 * Copyright 2003,2005,2006 Bernhard Walle <bernhard@bwalle.de>
 * Copyright 2010 Florian Rivoal <frivoal@gmail.com>
 * -------------------------------------------------------------------------------------------------
 *
 *  This program is free software; you can redistribute it and/or modify it
 *  under the terms of the GNU General Public License as published by the Free
 *  Software Foundation; either version 2 of the License, or (at your option)
 *  any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 675 Mass
 * Ave, Cambridge, MA 02139, USA.
 *
 * -------------------------------------------------------------------------------------------------
 */
#ifndef NET_H
#define NET_H

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#include <linux/limits.h>
#include <sys/time.h>

#define DISK_NAME_LENGTH 33

/* This structure stays the INFO variables */
typedef struct DataStats {
    double rd_bytes;
    double wr_bytes;
} DataStats;

typedef struct {
  double backup_in;
  double backup_out;
  double cur_in;
  double cur_out;
  int avail;
  int ssd;
  struct timeval prev_time;
  DataStats stats;
  char dev_name[DISK_NAME_LENGTH];
  char file_stats[PATH_MAX];
} diskdata;

/**
 * Initializes the diskspeed plugin. Used to set up inital values.
 * This function must be called after each change of the network interface.
 * @param   device      The network device, e.g. <code>ippp0</code> for ISDN on
 * Linux.
 * @return  <code>true</code> if no error occurs, <code>false</code> otherwise.
 * If there's an error, the error message may be set
 */
int init_diskspeed(diskdata *data, const char *device);

/**
 * Gets the current diskspeed. You must call init_diskspeed() once before you use
 * this function!
 * @param in        Input load in byte/s.
 * @param out       Output load in byte/s.
 * @param tot       Total load in byte/s.
 */
void get_current_diskspeed(diskdata *data, unsigned long *in,
                           unsigned long *out, unsigned long *tot);

/* 
 * Checks if the interface is exists and is up
 *
 * @param data Object
 * 
 * Returns TRUE if found and up, FALSE otherwise 
 */
int check_disk(diskdata*);

#endif /* NET_H */
