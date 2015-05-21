project('GStructureSimilarityChecker', 'c')

add_global_arguments('-std=c99', language : 'c')

extra_libs = ['-lm']

glib_dep = dependency('glib-2.0')
gobject_dep = dependency('gobject-2.0')
gst_video_dep = dependency('gstreamer-video-1.0', version: '>= 1.5')
gst_dep = dependency('gstreamer-1.0', version: '>= 1.5')
gdk_pixbuf_dep = dependency('gdk-pixbuf-2.0')

subdir('src')
subdir('tools')
