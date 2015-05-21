/* GStreamer
 *
 * Copyright (C) 2014 Mathieu Duponchelle <mathieu.duponchelle@opencreed.com>
 * Copyright (C) 2015 Raspberry Pi Foundation
 *  Author: Thibault Saunier <thibault.saunier@collabora.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "../src/gssim.h"

#include <gst/gst.h>
#include <gst/video/video.h>

int
main (int ac, char **av)
{
  Gssim *ssim;
  gfloat mssim = 0, lowest = 1, highest = -1;

  if (ac < 3)
    return -1;

  gst_init (&ac, &av);

  ssim = gssim_new ();

  gssim_compare_image_files (ssim, av[1], av[2], &mssim, &lowest, &highest);
  g_print ("the value is %f %f %f\n", mssim, lowest, highest);

  return 0;
}
