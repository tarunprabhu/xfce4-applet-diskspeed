/*
 * Copyright 2003,2005,2006 Bernhard Walle <bernhard@bwalle.de>
 * Copyright 2010 Florian Rivoal <frivoal@gmail.com>
 * Copyright 2017 Tarun Prabhu <tarun.prabhu@gmail.com>
 * ----------------------------------------------------------------------------
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
 * ----------------------------------------------------------------------------
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>

#include "disk.h"

#include <errno.h>
#include <sys/types.h>
#include <unistd.h>

/*****************************************************************************
 *
 * checkinterface()
 *
 * check if a given interface exists and is up.
 * return TRUE if found, FALSE if not
 *
 ****************************************************************************/

int check_disk(diskdata *data) {
  FILE *fp = NULL;
  char buf[5];

#ifdef DEBUG
  fprintf(stderr, "Checking the disk '%s' now ...\n",
          data->ifdata.if_name);
#endif

  if(access(data->file_stats, F_OK) == 0)
    return TRUE;
  return FALSE;
}

/******************************************************************************
 *
 * get_stat()
 *
 * read the network statistics from /proc/net/dev (PATH_NET_DEV)
 * if the file is not open open it. fseek() to the beginning and parse
 * each line until we've found the right interface
 *
 * returns 0 if successful, 1 in case of error
 *
 *****************************************************************************/

int get_stat(diskdata *data) {
  unsigned long dump;
  unsigned long rd = 0, wr = 0;
  FILE* fp = NULL;
 
  if(fp = fopen(data->file_stats, "r")) {
    fscanf(fp, "%ld %ld %ld %ld %ld %ld %ld %ld %ld %ld %ld", &dump, &dump, &rd,
           &dump, &dump, &dump, &wr, &dump, &dump, &dump, &dump);
    fclose(fp);

    data->stats.rd_bytes = rd * 512;
    data->stats.wr_bytes = wr * 512;
   
    return 0;
  }
  
  return 1;
}

/* -------------------------------------------------------------------------- */
int init_diskspeed(diskdata *data, const char *device) {
  const char* dir = "/sys/block/";
  memset(data, 0, sizeof(diskdata));

  if (device == NULL || strlen(device) == 0) {
    return TRUE;
  }

  g_strlcpy(data->dev_name, device, DISK_NAME_LENGTH);
  g_snprintf(data->file_stats, PATH_MAX,
           "%s/%s/%s", dir, device, "stat");
  
  if (check_disk(data) != TRUE) {
    data->avail = FALSE;
    return FALSE;
  }

  /* init in a sane state */
  get_stat(data);
  data->backup_in = data->stats.rd_bytes;
  data->backup_out = data->stats.wr_bytes;

  data->avail = TRUE;

  DBG("The diskspeed plugin was initialized for '%s'.", device);

  return TRUE;
}

/* -------------------------------------------------------------------------- */
void get_current_diskspeed(diskdata *data, unsigned long *in, unsigned long *out,
                         unsigned long *tot) {
  struct timeval curr_time;
  double delta_t;

  if (!data->avail) {
    if (in != NULL && out != NULL && tot != NULL) {
      *in = *out = *tot = 0;
    }
  }

  gettimeofday(&curr_time, NULL);

  delta_t = (double)((curr_time.tv_sec - data->prev_time.tv_sec) * 1000000L +
                     (curr_time.tv_usec - data->prev_time.tv_usec)) /
            1000000.0;

  /* update */
  get_stat(data);
  if (data->backup_in > data->stats.rd_bytes) {
    data->cur_in = (int)(data->stats.rd_bytes / delta_t + 0.5);
  } else {
    data->cur_in =
        (int)((data->stats.rd_bytes - data->backup_in) / delta_t + 0.5);
  }

  if (data->backup_out > data->stats.wr_bytes) {
    data->cur_out = (int)(data->stats.wr_bytes / delta_t + 0.5);
  } else {
    data->cur_out =
        (int)((data->stats.wr_bytes - data->backup_out) / delta_t + 0.5);
  }

  if (in != NULL && out != NULL && tot != NULL) {
    *in = data->cur_in;
    *out = data->cur_out;
    *tot = *in + *out;
  }

  /* save 'new old' values */
  data->backup_in = data->stats.rd_bytes;
  data->backup_out = data->stats.wr_bytes;

  /* do the same with time */
  data->prev_time.tv_sec = curr_time.tv_sec;
  data->prev_time.tv_usec = curr_time.tv_usec;
}

