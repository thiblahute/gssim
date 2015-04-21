#include <glib.h>
#include <math.h>
#include <stdlib.h>

typedef struct _SSimWindowCache {
  gint x_window_start;
  gint x_weight_start;
  gint x_window_end;
  gint y_window_start;
  gint y_weight_start;
  gint y_window_end;
  gfloat element_summ;
} SSimWindowCache;

typedef struct _SSim
{
  gint width;
  gint height;
  gint windowsize;
  gint            windowtype;
  SSimWindowCache *windows;
  gfloat         *weights;
  gfloat         const1;
  gfloat         const2;
  gfloat          sigma;
} SSim;

typedef gfloat (*SSimWeightFunc) (SSim * ssim, gint y, gint x);
  
static void
calculate_mu (SSim * ssim, gfloat * outmu, guint8 * buf)
{
  gint oy, ox, iy, ix;

  for (oy = 0; oy < ssim->height; oy++) {
    for (ox = 0; ox < ssim->width; ox++) {
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

      source_offset = oy * ssim->width + ox;

      winstart_x = ssim->windows[source_offset].x_window_start;
      wghstart_x = ssim->windows[source_offset].x_weight_start;
      winend_x = ssim->windows[source_offset].x_window_end;
      winstart_y = ssim->windows[source_offset].y_window_start;
      wghstart_y = ssim->windows[source_offset].y_weight_start;
      winend_y = ssim->windows[source_offset].y_window_end;
      elsumm = ssim->windows[source_offset].element_summ;

      switch (ssim->windowtype) {
        case 0:
          for (iy = winstart_y; iy <= winend_y; iy++) {
            pixel_offset = iy * ssim->width;
            for (ix = winstart_x; ix <= winend_x; ix++)
              mu += buf[pixel_offset + ix];
          }
          mu = mu / elsumm;
          break;
        case 1:

          weight_y_base = wghstart_y - winstart_y;
          weight_x_base = wghstart_x - winstart_x;

          for (iy = winstart_y; iy <= winend_y; iy++) {
            pixel_offset = iy * ssim->width;
            weight_offset = (weight_y_base + iy) * ssim->windowsize +
                weight_x_base;
            for (ix = winstart_x; ix <= winend_x; ix++) {
              weight = ssim->weights[weight_offset + ix];
              mu += weight * buf[pixel_offset + ix];
            }
          }
          mu = mu / elsumm;
          break;
      }
      outmu[oy * ssim->width + ox] = mu;
    }
  }

}

static void
calcssim_canonical (SSim * ssim, guint8 * org, gfloat * orgmu, guint8 * mod,
    guint8 * out, gfloat * mean, gfloat * lowest, gfloat * highest)
{
  gint oy, ox, iy, ix;
  gfloat cumulative_ssim = 0;
  *lowest = G_MAXFLOAT;
  *highest = -G_MAXFLOAT;

  for (oy = 0; oy < ssim->height; oy++) {
    for (ox = 0; ox < ssim->width; ox++) {
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

      source_offset = oy * ssim->width + ox;

      winstart_x = ssim->windows[source_offset].x_window_start;
      wghstart_x = ssim->windows[source_offset].x_weight_start;
      winend_x = ssim->windows[source_offset].x_window_end;
      winstart_y = ssim->windows[source_offset].y_window_start;
      wghstart_y = ssim->windows[source_offset].y_weight_start;
      winend_y = ssim->windows[source_offset].y_window_end;
      elsumm = ssim->windows[source_offset].element_summ;

      switch (ssim->windowtype) {
        case 0:
          for (iy = winstart_y; iy <= winend_y; iy++) {
            pixel_offset = iy * ssim->width;
            for (ix = winstart_x; ix <= winend_x; ix++) {
              mu_m += mod[pixel_offset + ix];
            }
          }
          mu_m = mu_m / elsumm;
          mu_o = orgmu[oy * ssim->width + ox];
          for (iy = winstart_y; iy <= winend_y; iy++) {
            pixel_offset = iy * ssim->width;
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
            pixel_offset = iy * ssim->width;
            weight_offset = (weight_y_base + iy) * ssim->windowsize +
                weight_x_base;
            for (ix = winstart_x; ix <= winend_x; ix++) {
              weight = ssim->weights[weight_offset + ix];
              mu_o += weight * org[pixel_offset + ix];
              mu_m += weight * mod[pixel_offset + ix];
            }
          }
          mu_m = mu_m / elsumm;
          mu_o = orgmu[oy * ssim->width + ox];
          for (iy = winstart_y; iy <= winend_y; iy++) {
            gfloat *weights_with_offset;
            guint8 *org_with_offset, *mod_with_offset;
            gfloat wt1, wt2;
            pixel_offset = iy * ssim->width;
            weight_offset = (weight_y_base + iy) * ssim->windowsize +
                weight_x_base;
            weights_with_offset = &ssim->weights[weight_offset];
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
      tmp1 = (2 * mu_o * mu_m + ssim->const1) * (2 * sigma_om + ssim->const2) /
          ((mu_o * mu_o + mu_m * mu_m + ssim->const1) *
          (sigma_o * sigma_o + sigma_m * sigma_m + ssim->const2));

      /* SSIM can go negative, that's why it is
         127 + index * 128 instead of index * 255 */
      if (out)
        out[oy * ssim->width + ox] = 127 + tmp1 * 128;
      *lowest = MIN (*lowest, tmp1);
      *highest = MAX (*highest, tmp1);
      cumulative_ssim += tmp1;
    }
  }
  *mean = cumulative_ssim / (ssim->width * ssim->height);
}

static gfloat
ssim_weight_func_none (SSim * ssim, gint y, gint x)
{
  return 1;
}

static gfloat
ssim_weight_func_gauss (SSim * ssim, gint y, gint x)
{
  gfloat coord = sqrt (x * x + y * y);
  return exp (-1 * (coord * coord) / (2 * ssim->sigma * ssim->sigma)) /
      (ssim->sigma * sqrt (2 * G_PI));
}

static gboolean
ssim_regenerate_windows (SSim * ssim)
{
  gint windowiseven;
  gint y, x, y2, x2;
  SSimWeightFunc func;
  gfloat normal_summ = 0;
  gint normal_count = 0;

  g_free (ssim->weights);

  ssim->weights = g_new (gfloat, ssim->windowsize * ssim->windowsize);

  windowiseven = ((gint) ssim->windowsize / 2) * 2 == ssim->windowsize ? 1 : 0;

  g_free (ssim->windows);

  ssim->windows = g_new (SSimWindowCache, ssim->height * ssim->width);

  switch (ssim->windowtype) {
    case 0:
      func = ssim_weight_func_none;
      break;
    case 1:
      func = ssim_weight_func_gauss;
      break;
    default:
      ssim->windowtype = 1;
      func = ssim_weight_func_gauss;
  }

  for (y = 0; y < ssim->windowsize; y++) {
    gint yoffset = y * ssim->windowsize;
    for (x = 0; x < ssim->windowsize; x++) {
      ssim->weights[yoffset + x] = func (ssim, x - ssim->windowsize / 2 +
          windowiseven, y - ssim->windowsize / 2 + windowiseven);
      normal_summ += ssim->weights[yoffset + x];
      normal_count++;
    }
  }

  for (y = 0; y < ssim->height; y++) {
    for (x = 0; x < ssim->width; x++) {
      SSimWindowCache win;
      gint element_count = 0;

      win.x_window_start = x - ssim->windowsize / 2 + windowiseven;
      win.x_weight_start = 0;
      if (win.x_window_start < 0) {
        win.x_weight_start = -win.x_window_start;
        win.x_window_start = 0;
      }

      win.x_window_end = x + ssim->windowsize / 2;
      if (win.x_window_end >= ssim->width)
        win.x_window_end = ssim->width - 1;

      win.y_window_start = y - ssim->windowsize / 2 + windowiseven;
      win.y_weight_start = 0;
      if (win.y_window_start < 0) {
        win.y_weight_start = -win.y_window_start;
        win.y_window_start = 0;
      }

      win.y_window_end = y + ssim->windowsize / 2;
      if (win.y_window_end >= ssim->height)
        win.y_window_end = ssim->height - 1;

      win.element_summ = 0;
      element_count = (win.y_window_end - win.y_window_start + 1) *
          (win.x_window_end - win.x_window_start + 1);
      if (element_count == normal_count)
        win.element_summ = normal_summ;
      else {
        for (y2 = win.y_weight_start; y2 < ssim->windowsize; y2++) {
          for (x2 = win.x_weight_start; x2 < ssim->windowsize; x2++) {
            win.element_summ += ssim->weights[y2 * ssim->windowsize + x2];
          }
        }
      }
      ssim->windows[(y * ssim->width + x)] = win;
    }
  }

  /* FIXME: while 0.01 and 0.03 are pretty much static, the 255 implies that
   * we're working with 8-bit-per-color-component format, which may not be true
   */
  ssim->const1 = 0.01 * 255 * 0.01 * 255;
  ssim->const2 = 0.03 * 255 * 0.03 * 255;
  return TRUE;
}

init_ssim (SSim *ssim) {
  ssim->windowsize = 11;
  ssim->windowtype = 1;
  ssim->windows = NULL;
  ssim->sigma = 1.5;
}

int main (int ac, char **av)
{
  gfloat *orgmu = NULL;
  SSim *ssim = g_malloc0 (sizeof(SSim));
  gfloat mssim = 0, lowest = 1, highest = -1;
  GMappedFile *ref_mapped_file, *cmp_mapped_file;
  GError *error = NULL;
  guint8 *ref_data, *cmp_data;

  if (ac < 5)
    return EXIT_FAILURE;

  ref_mapped_file = g_mapped_file_new (av[1], FALSE, &error);
  cmp_mapped_file = g_mapped_file_new (av[2], FALSE, &error);

  init_ssim (ssim);
  ssim->width = (gint) g_ascii_strtoll (av[3], NULL, 10);
  ssim->height = (gint) g_ascii_strtoll (av[4], NULL, 10);

  orgmu = g_new (gfloat, ssim->width * ssim->height);

  ssim_regenerate_windows (ssim);

  ref_data = (guint8 *) g_mapped_file_get_contents (ref_mapped_file);
  cmp_data = (guint8 *) g_mapped_file_get_contents (cmp_mapped_file);

  calculate_mu (ssim, orgmu, ref_data);
  g_print ("the value is %f\n", mssim);
  calcssim_canonical (ssim, ref_data, orgmu, cmp_data, NULL, &mssim, &lowest, &highest);
  g_mapped_file_unref (ref_mapped_file);
  g_mapped_file_unref (cmp_mapped_file);
  g_free (orgmu);

  g_print ("the value is %f\n", mssim);

  return EXIT_SUCCESS;
}
