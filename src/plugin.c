#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "gstevtest.h"
#include "gsttestsrc.h"

G_BEGIN_DECLS

static gboolean plugin_init(GstPlugin* plugin)
{
	gboolean ret = TRUE;

	ret &= gst_element_register(plugin, "evtest", GST_RANK_NONE,
	                            GST_TYPE_EVTEST);

	ret &= gst_element_register(plugin, "testsrc", GST_RANK_NONE,
	                            GST_TYPE_TESTSRC);

	return ret;
}

// clang-format off
GST_PLUGIN_DEFINE(
	GST_VERSION_MAJOR,
	GST_VERSION_MINOR,
	demo,
	"Gstreamer plugin demos",
	plugin_init,
	VERSION,
	"LGPL",
	"gstdemo",
	"https://github.com/varphone/gst-plugins-demo/"
);
// clang-format on

G_END_DECLS
