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
#include <glib-object.h>
#include <glib.h>
#include <math.h>
#include <stdlib.h>

#include "gssim.h"

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gst/gst.h>
#include <gst/video/video.h>

typedef struct _SSimWindowCache
{
  gint x_window_start;
  gint x_weight_start;
  gint x_window_end;
  gint y_window_start;
  gint y_weight_start;
  gint y_window_end;
  gfloat element_summ;
} SSimWindowCache;

G_DEFINE_TYPE (Gssim, gssim, G_TYPE_OBJECT)

enum
{
  PROP_FIRST_PROP = 1,
  N_PROPS
};

struct _GssimPriv
{
  gint width;
  gint height;
  gint windowsize;
  gint windowtype;
  SSimWindowCache *windows;
  gfloat *weights;
  gfloat const1;
  gfloat const2;
  gfloat sigma;

  gfloat *orgmu;

  GstVideoConverter *converter;
  GstVideoInfo in_info, out_info;
};

typedef gfloat (*SSimWeightFunc) (Gssim * self, gint y, gint x);

void
gssim_calculate_mu (Gssim * self, guint8 * buf)
{
  gint oy, ox, iy, ix;

  for (oy = 0; oy < self->priv->height; oy++) {
    for (ox = 0; ox < self->priv->width; ox++) {
      gfloat mu = 0;
      gfloat elsumm;
      gint weight_y_base, weight_x_base;
      gint weight_offset;
      gint pixel_offset;
      gint winstart_y;
      gint wghstart_y;
      gint winend_y;
      gint winstart_x;
      gint wghstart_x;
      gint winend_x;
      gfloat weight;
      gint source_offset;

      source_offset = oy * self->priv->width + ox;

      winstart_x = self->priv->windows[source_offset].x_window_start;
      wghstart_x = self->priv->windows[source_offset].x_weight_start;
      winend_x = self->priv->windows[source_offset].x_window_end;
      winstart_y = self->priv->windows[source_offset].y_window_start;
      wghstart_y = self->priv->windows[source_offset].y_weight_start;
      winend_y = self->priv->windows[source_offset].y_window_end;
      elsumm = self->priv->windows[source_offset].element_summ;

      switch (self->priv->windowtype) {
        case 0:
          for (iy = winstart_y; iy <= winend_y; iy++) {
            pixel_offset = iy * self->priv->width;
            for (ix = winstart_x; ix <= winend_x; ix++)
              mu += buf[pixel_offset + ix];
          }
          mu = mu / elsumm;
          break;
        case 1:

          weight_y_base = wghstart_y - winstart_y;
          weight_x_base = wghstart_x - winstart_x;

          for (iy = winstart_y; iy <= winend_y; iy++) {
            pixel_offset = iy * self->priv->width;
            weight_offset = (weight_y_base + iy) * self->priv->windowsize +
                weight_x_base;
            for (ix = winstart_x; ix <= winend_x; ix++) {
              weight = self->priv->weights[weight_offset + ix];
              mu += weight * buf[pixel_offset + ix];
            }
          }
          mu = mu / elsumm;
          break;
      }
      self->priv->orgmu[oy * self->priv->width + ox] = mu;
    }
  }
}

void
gssim_cal_canonical (Gssim * self, guint8 * org, guint8 * mod,
    guint8 * out, gfloat * mean, gfloat * lowest, gfloat * highest)
{
  gint oy, ox, iy, ix;
  gfloat cumulative_ssim = 0;
  *lowest = G_MAXFLOAT;
  *highest = -G_MAXFLOAT;

  for (oy = 0; oy < self->priv->height; oy++) {
    for (ox = 0; ox < self->priv->width; ox++) {
      gfloat mu_o = 0, mu_m = 0;
      gdouble sigma_o = 0, sigma_m = 0, sigma_om = 0;
      gfloat tmp1, tmp2;
      gfloat elsumm = 0;
      gint weight_y_base, weight_x_base;
      gint weight_offset;
      gint pixel_offset;
      gint winstart_y;
      gint wghstart_y;
      gint winend_y;
      gint winstart_x;
      gint wghstart_x;
      gint winend_x;
      gfloat weight;
      gint source_offset;

      source_offset = oy * self->priv->width + ox;

      winstart_x = self->priv->windows[source_offset].x_window_start;
      wghstart_x = self->priv->windows[source_offset].x_weight_start;
      winend_x = self->priv->windows[source_offset].x_window_end;
      winstart_y = self->priv->windows[source_offset].y_window_start;
      wghstart_y = self->priv->windows[source_offset].y_weight_start;
      winend_y = self->priv->windows[source_offset].y_window_end;
      elsumm = self->priv->windows[source_offset].element_summ;

      switch (self->priv->windowtype) {
        case 0:
          for (iy = winstart_y; iy <= winend_y; iy++) {
            pixel_offset = iy * self->priv->width;
            for (ix = winstart_x; ix <= winend_x; ix++) {
              mu_m += mod[pixel_offset + ix];
            }
          }
          mu_m = mu_m / elsumm;
          mu_o = self->priv->orgmu[oy * self->priv->width + ox];
          for (iy = winstart_y; iy <= winend_y; iy++) {
            pixel_offset = iy * self->priv->width;
            for (ix = winstart_x; ix <= winend_x; ix++) {
              tmp1 = org[pixel_offset + ix] - mu_o;
              tmp2 = mod[pixel_offset + ix] - mu_m;
              sigma_o += tmp1 * tmp1;
              sigma_m += tmp2 * tmp2;
              sigma_om += tmp1 * tmp2;
            }
          }
          break;
        case 1:

          weight_y_base = wghstart_y - winstart_y;
          weight_x_base = wghstart_x - winstart_x;

          for (iy = winstart_y; iy <= winend_y; iy++) {
            pixel_offset = iy * self->priv->width;
            weight_offset = (weight_y_base + iy) * self->priv->windowsize +
                weight_x_base;
            for (ix = winstart_x; ix <= winend_x; ix++) {
              weight = self->priv->weights[weight_offset + ix];
              mu_o += weight * org[pixel_offset + ix];
              mu_m += weight * mod[pixel_offset + ix];
            }
          }
          mu_m = mu_m / elsumm;
          mu_o = self->priv->orgmu[oy * self->priv->width + ox];
          for (iy = winstart_y; iy <= winend_y; iy++) {
            gfloat *weights_with_offset;
            guint8 *org_with_offset, *mod_with_offset;
            gfloat wt1, wt2;
            pixel_offset = iy * self->priv->width;
            weight_offset = (weight_y_base + iy) * self->priv->windowsize +
                weight_x_base;
            weights_with_offset = &self->priv->weights[weight_offset];
            org_with_offset = &org[pixel_offset];
            mod_with_offset = &mod[pixel_offset];
            for (ix = winstart_x; ix <= winend_x; ix++) {
              weight = weights_with_offset[ix];
              tmp1 = org_with_offset[ix] - mu_o;
              tmp2 = mod_with_offset[ix] - mu_m;
              wt1 = weight * tmp1;
              wt2 = weight * tmp2;
              sigma_o += wt1 * tmp1;
              sigma_m += wt2 * tmp2;
              sigma_om += wt1 * tmp2;
            }
          }
          break;
      }
      sigma_o = sqrt (sigma_o / elsumm);
      sigma_m = sqrt (sigma_m / elsumm);
      sigma_om = sigma_om / elsumm;
      tmp1 = (2 * mu_o * mu_m + self->priv->const1) * (2 * sigma_om + self->priv->const2) /
          ((mu_o * mu_o + mu_m * mu_m + self->priv->const1) *
          (sigma_o * sigma_o + sigma_m * sigma_m + self->priv->const2));

      /* SSIM can go negative, that's why it is
         127 + index * 128 instead of index * 255 */
      if (out)
        out[oy * self->priv->width + ox] = 127 + tmp1 * 128;
      *lowest = MIN (*lowest, tmp1);
      *highest = MAX (*highest, tmp1);
      cumulative_ssim += tmp1;
    }
  }
  *mean = cumulative_ssim / (self->priv->width * self->priv->height);
}

static gfloat
ssim_weight_func_none (Gssim * self, gint y, gint x)
{
  return 1;
}

static gfloat
ssim_weight_func_gauss (Gssim * self, gint y, gint x)
{
  gfloat coord = sqrt (x * x + y * y);
  return exp (-1 * (coord * coord) / (2 * self->priv->sigma * self->priv->sigma)) /
      (self->priv->sigma * sqrt (2 * G_PI));
}

gboolean
gssim_regenerate_windows (Gssim * self)
{
  gint windowiseven;
  gint y, x, y2, x2;
  SSimWeightFunc func;
  gfloat normal_summ = 0;
  gint normal_count = 0;

  g_free (self->priv->weights);

  self->priv->weights = g_new (gfloat, self->priv->windowsize * self->priv->windowsize);

  windowiseven = ((gint) self->priv->windowsize / 2) * 2 == self->priv->windowsize ? 1 : 0;

  g_free (self->priv->windows);

  self->priv->windows = g_new (SSimWindowCache, self->priv->height * self->priv->width);

  switch (self->priv->windowtype) {
    case 0:
      func = ssim_weight_func_none;
      break;
    case 1:
      func = ssim_weight_func_gauss;
      break;
    default:
      self->priv->windowtype = 1;
      func = ssim_weight_func_gauss;
  }

  for (y = 0; y < self->priv->windowsize; y++) {
    gint yoffset = y * self->priv->windowsize;
    for (x = 0; x < self->priv->windowsize; x++) {
      self->priv->weights[yoffset + x] = func (self, x - self->priv->windowsize / 2 +
          windowiseven, y - self->priv->windowsize / 2 + windowiseven);
      normal_summ += self->priv->weights[yoffset + x];
      normal_count++;
    }
  }

  for (y = 0; y < self->priv->height; y++) {
    for (x = 0; x < self->priv->width; x++) {
      SSimWindowCache win;
      gint element_count = 0;

      win.x_window_start = x - self->priv->windowsize / 2 + windowiseven;
      win.x_weight_start = 0;
      if (win.x_window_start < 0) {
        win.x_weight_start = -win.x_window_start;
        win.x_window_start = 0;
      }

      win.x_window_end = x + self->priv->windowsize / 2;
      if (win.x_window_end >= self->priv->width)
        win.x_window_end = self->priv->width - 1;

      win.y_window_start = y - self->priv->windowsize / 2 + windowiseven;
      win.y_weight_start = 0;
      if (win.y_window_start < 0) {
        win.y_weight_start = -win.y_window_start;
        win.y_window_start = 0;
      }

      win.y_window_end = y + self->priv->windowsize / 2;
      if (win.y_window_end >= self->priv->height)
        win.y_window_end = self->priv->height - 1;

      win.element_summ = 0;
      element_count = (win.y_window_end - win.y_window_start + 1) *
          (win.x_window_end - win.x_window_start + 1);
      if (element_count == normal_count)
        win.element_summ = normal_summ;
      else {
        for (y2 = win.y_weight_start; y2 < self->priv->windowsize; y2++) {
          for (x2 = win.x_weight_start; x2 < self->priv->windowsize; x2++) {
            win.element_summ += self->priv->weights[y2 * self->priv->windowsize + x2];
          }
        }
      }
      self->priv->windows[(y * self->priv->width + x)] = win;
    }
  }

  /* FIXME: while 0.01 and 0.03 are pretty much static, the 255 implies that
   * we're working with 8-bit-per-color-component format, which may not be true
   */
  self->priv->const1 = 0.01 * 255 * 0.01 * 255;
  self->priv->const2 = 0.03 * 255 * 0.03 * 255;
  return TRUE;
}

gboolean
gssim_convert (Gssim *self, GstVideoFrame *frame, guint8 * data, gsize length)
{
  gboolean res = TRUE;
  GstBuffer *outbuf;
  GstVideoFrame inframe;
  GstBuffer *buf = gst_buffer_new_wrapped (data, length);

  if (!gst_video_frame_map (&inframe, &self->priv->in_info, buf, GST_MAP_READ)) {
    GST_ERROR ("Could not map input buffer");
    goto fail;
  }

  outbuf = gst_buffer_new_allocate (NULL, self->priv->out_info.size, NULL);
  if (!gst_video_frame_map (frame,
        &self->priv->out_info, outbuf, GST_MAP_WRITE)) {
    GST_ERROR ("Could not map input buffer");
    goto fail;
  }

  gst_video_converter_frame (self->priv->converter, &inframe, frame);

done:
  return res;

fail:
  res = FALSE;
  goto done;
}

static void
gssim_configure (Gssim *self, GstVideoFormat in_format, gint width, gint height)
{
  self->priv->width = width;
  self->priv->height = height;

  self->priv->orgmu = g_new (gfloat, width * height);
  gst_video_info_init (&self->priv->in_info);
  gst_video_info_set_format (&self->priv->in_info, in_format,
      self->priv->width, self->priv->height);

  gst_video_info_init (&self->priv->out_info);
  gst_video_info_set_format (&self->priv->out_info, GST_VIDEO_FORMAT_I420,
      self->priv->width, self->priv->height);

  self->priv->converter = gst_video_converter_new (&self->priv->in_info, &self->priv->out_info,
      NULL);
}

gboolean
gssim_compare_image_files (Gssim *self, const gchar *ref_file, const gchar * file, gfloat * mean,
    gfloat * lowest, gfloat * highest)
{
  guint size1, size2;
  GdkPixbuf *pb1, *pb2;
  GError *error = NULL;
  guint8 *data1, *data2;

  GstVideoFormat format;
  GstMapInfo map1, map2;
  GstVideoFrame converted_frame1, converted_frame2;

  pb1 = gdk_pixbuf_new_from_file (ref_file, &error);
  if (error) {
    GST_ERROR ("Could not open %s", ref_file);

    return FALSE;
  }

  pb2 = gdk_pixbuf_new_from_file (file, &error);
  if (error) {
    GST_ERROR ("Could not open %s", file);

    return FALSE;
  }

  if (gdk_pixbuf_get_width (pb1) != gdk_pixbuf_get_width (pb2)) {
    GST_ERROR ("Images do not have the same width : %d != %d",
        gdk_pixbuf_get_width (pb1), gdk_pixbuf_get_width (pb2));

    return FALSE;
  }

  if (gdk_pixbuf_get_height (pb1) != gdk_pixbuf_get_height (pb2)) {
    GST_ERROR ("Images do not have the same height : %d != %d",
        gdk_pixbuf_get_height (pb1), gdk_pixbuf_get_height (pb2));

    return -1;
  }

  if (gdk_pixbuf_get_has_alpha (pb1) != gdk_pixbuf_get_has_alpha (pb2)) {
    GST_ERROR ("Images do not have the same alpha property");

    return -1;
  }

#if G_BYTE_ORDER == G_BIG_ENDIAN
  if (gdk_pixbuf_get_has_alpha (pb1))
    format = GST_VIDEO_FORMAT_BGRA;
  else
    format = GST_VIDEO_FORMAT_BGR;
#else
  if (gdk_pixbuf_get_has_alpha (pb1))
    format = GST_VIDEO_FORMAT_ARGB;
  else
    format = GST_VIDEO_FORMAT_RGB;
#endif

  gssim_configure (self, format, gdk_pixbuf_get_width (pb1),
      gdk_pixbuf_get_height (pb1));

  data1 = gdk_pixbuf_get_pixels_with_length (pb1, &size1);
  gssim_convert (self, &converted_frame1, data1, size1);
  if (!gst_buffer_map (converted_frame1.buffer, &map1, GST_MAP_READ)) {
    GST_ERROR ("Could not map first buffer");

    return -1;
  }

  data2 = gdk_pixbuf_get_pixels_with_length (pb2, &size2);
  gssim_convert (self, &converted_frame2, data2, size2);
  if (!gst_buffer_map (converted_frame2.buffer, &map2, GST_MAP_READ)) {
    GST_ERROR ("Could not map first buffer");

    return -1;
  }

  gssim_regenerate_windows (self);
  gssim_calculate_mu (self, map1.data);
  gssim_cal_canonical (self, map1.data, map2.data, NULL, mean, lowest,
      highest);

  g_object_unref (pb1);
  g_object_unref (pb2);

}

static void
gssim_get_property (GObject *object,
    guint property_id,
    GValue *value,
    GParamSpec *pspec)
{
  //Gssim *self = GSSIM (object);

  switch (property_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gssim_set_property (GObject *object,
    guint property_id,
    const GValue *value,
    GParamSpec *pspec)
{
  //Gssim *self = GSSIM (object);

  switch (property_id)
    {
      default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
        break;
    }
}

static void
gssim_constructed (GObject *object)
{
  //Gssim *self = GSSIM (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) gssim_parent_class)->constructed;

  chain_up (object);
}

static void
gssim_dispose (GObject *object)
{
  //Gssim *self = GSSIM (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) gssim_parent_class)->dispose;

  chain_up (object);
}

static void
gssim_finalize (GObject *object)
{
  Gssim *self = GSSIM (object);
  void (*chain_up) (GObject *) =
      ((GObjectClass *) gssim_parent_class)->finalize;

  g_free (self->priv->orgmu);

  chain_up (object);
}

static void
gssim_class_init (
    GssimClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS (klass);
  //GParamSpec *spec;

  oclass->get_property = gssim_get_property;
  oclass->set_property = gssim_set_property;
  oclass->constructed = gssim_constructed;
  oclass->dispose = gssim_dispose;
  oclass->finalize = gssim_finalize;

  g_type_class_add_private (klass, sizeof (GssimPriv));
}

static void
gssim_init (Gssim *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
      GSSIM_TYPE, GssimPriv);

  self->priv->windowsize = 11;
  self->priv->windowtype = 1;
  self->priv->windows = NULL;
  self->priv->sigma = 1.5;
}

Gssim *
gssim_new (void)
{
  return g_object_new (GSSIM_TYPE, NULL);
}
