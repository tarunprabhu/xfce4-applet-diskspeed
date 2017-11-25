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

#include "net.h"

#include <errno.h>
#include <sys/types.h>

/*****************************************************************************
 *
 * checkinterface()
 *
 * check if a given interface exists and is up.
 * return TRUE if found, FALSE if not
 *
 ****************************************************************************/

int check_interface(netdata *data) {
  FILE *fp = NULL;
  char buf[5];

#ifdef DEBUG
  fprintf(stderr, "Checking the interface '%s' now ...\n",
          data->ifdata.if_name);
#endif

  if (fp = fopen(data->file_operstate, "r")) {
    fgets(buf, 4, fp);
    if (strncmp(buf, "up", 4))
      return TRUE;
    fclose(fp);
  }

  return FALSE;
}

/*
 * read_file()
 *
 * Read the specified file which will contain a single integer value
 *
 * Returns 0 if successful, 1 otherwise
 */
static int read_file(const char *filename, unsigned long *out) {
  FILE *fp = NULL;
  if (fp = fopen(filename, "r")) {
    fscanf(fp, "%ld", out);
    fclose(fp);
    return 0;
  }

  return 1;
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

int get_stat(netdata *data) {
  int read_tx, read_rx;
  unsigned long rx = 0, tx = 0;

  read_tx = read_file(data->file_rx_bytes, &rx);
  read_rx = read_file(data->file_tx_bytes, &tx);

  if ((read_rx == 0) && (read_tx == 0)) {
    data->stats.rx_bytes = rx;
    data->stats.tx_bytes = tx;
    return 0;
  }

  return 1;
}

/* -------------------------------------------------------------------------- */
int init_netload(netdata *data, const char *device) {
  const char* dir = "/sys/class/net/";
  memset(data, 0, sizeof(netdata));

  if (device == NULL || strlen(device) == 0) {
    return TRUE;
  }

  data->ip_update_count = 0;
  data->up = FALSE;
  data->up_update_count = 0;

  g_strlcpy(data->if_name, device, INTERFACE_NAME_LENGTH);
  g_snprintf(data->file_rx_bytes, PATH_MAX,
           "%s/%s/statistics/%s", dir, device, "rx_bytes");
  g_snprintf(data->file_tx_bytes, PATH_MAX,
           "%s/%s/statistics/%s", dir, device, "tx_bytes");
  g_snprintf(data->file_operstate, PATH_MAX,
           "%s/%s/operstate", dir, device, "operstate");
  
  if (check_interface(data) != TRUE) {
    data->correct_interface = FALSE;
    return FALSE;
  }

  /* init in a sane state */
  get_stat(data);
  data->backup_in = data->stats.rx_bytes;
  data->backup_out = data->stats.tx_bytes;

  data->correct_interface = TRUE;

  DBG("The netload plugin was initialized for '%s'.", device);

  return TRUE;
}

/* -------------------------------------------------------------------------- */
void get_current_netload(netdata *data, unsigned long *in, unsigned long *out,
                         unsigned long *tot) {
  struct timeval curr_time;
  double delta_t;

  if (!data->correct_interface) {
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
  if (data->backup_in > data->stats.rx_bytes) {
    data->cur_in = (int)(data->stats.rx_bytes / delta_t + 0.5);
  } else {
    data->cur_in =
        (int)((data->stats.rx_bytes - data->backup_in) / delta_t + 0.5);
  }

  if (data->backup_out > data->stats.tx_bytes) {
    data->cur_out = (int)(data->stats.tx_bytes / delta_t + 0.5);
  } else {
    data->cur_out =
        (int)((data->stats.tx_bytes - data->backup_out) / delta_t + 0.5);
  }

  if (in != NULL && out != NULL && tot != NULL) {
    *in = data->cur_in;
    *out = data->cur_out;
    *tot = *in + *out;
  }

  /* save 'new old' values */
  data->backup_in = data->stats.rx_bytes;
  data->backup_out = data->stats.tx_bytes;

  /* do the same with time */
  data->prev_time.tv_sec = curr_time.tv_sec;
  data->prev_time.tv_usec = curr_time.tv_usec;
}

/* -------------------------------------------------------------------------- */
char *get_name(netdata *data) {
  return data->if_name;
}

/* -------------------------------------------------------------------------- */
void close_netload(netdata *data) {
  ;
}
