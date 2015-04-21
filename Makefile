all: ssim_compare_yuv.c
	gcc ssim_compare_yuv.c `pkg-config glib-2.0 --cflags --libs` -o ssim_compare_yuv -lm
