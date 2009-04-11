/*
 *  Callouts
 *  Copyright (C) 2009 Andreas Öman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "showtime.h"

static LIST_HEAD(, callout) callouts;

static hts_mutex_t callout_mutex;
static hts_cond_t callout_cond;


/**
 *
 */
static int
calloutcmp(callout_t *a, callout_t *b)
{
  if(a->c_expire < b->c_expire)
    return -1;
  else if(a->c_expire > b->c_expire)
    return 1;
 return 0;
}


/**
 *
 */
void
callout_arm_abs(callout_t *d, callout_callback_t *callback, void *opaque,
		 time_t when)
{
  hts_mutex_lock(&callout_mutex);

  if(d == NULL)
    d = malloc(sizeof(callout_t));
  else if(d->c_callback != NULL)
    LIST_REMOVE(d, c_link);
    
  d->c_callback = callback;
  d->c_opaque = opaque;
  d->c_expire = when;

  LIST_INSERT_SORTED(&callouts, d, c_link, calloutcmp);

  hts_cond_signal(&callout_cond);
  hts_mutex_unlock(&callout_mutex);
}

/**
 *
 */
void
callout_arm(callout_t *d, callout_callback_t *callback,
	     void *opaque, int delta)
{
  time_t now;
  time(&now);
  
  callout_arm_abs(d, callback, opaque, now + delta);
}

/**
 *
 */
void
callout_disarm(callout_t *d)
{
  hts_mutex_lock(&callout_mutex);
  if(d->c_callback) {
    LIST_REMOVE(d, c_link);
    d->c_callback = NULL;
  }
  hts_mutex_unlock(&callout_mutex);
}


/**
 *
 */
static void *
callout_loop(void *aux)
{
  time_t now;
  struct timespec ts;
  callout_t *c;
  callout_callback_t *cc;

  hts_mutex_init(&callout_mutex);
  hts_cond_init(&callout_cond);

  while(1) {

    time(&now);
  
    while((c = LIST_FIRST(&callouts)) != NULL && c->c_expire <= now) {
      cc = c->c_callback;
      LIST_REMOVE(c, c_link);
      c->c_callback = NULL;
      cc(c, c->c_opaque);
    }

    if((c = LIST_FIRST(&callouts)) != NULL) {
      ts.tv_sec = c->c_expire;
      hts_cond_wait_timeout(&callout_cond, &callout_mutex, &ts);
    } else {
      hts_cond_wait(&callout_cond, &callout_mutex);
    }
  }
}


/**
 *
 */
void
callout_init(void)
{
  hts_mutex_init(&callout_mutex);
  hts_cond_init(&callout_cond);

  hts_thread_create_detached(callout_loop, NULL);
}