#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gstevtest.h"

GST_DEBUG_CATEGORY_STATIC(gst_evtest_debug);
#define GST_CAT_DEFAULT gst_evtest_debug

/* Filter signals and args */
enum
{
	/* FILL ME */
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_SILENT
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE(
        "sink", GST_PAD_SINK, GST_PAD_ALWAYS, GST_STATIC_CAPS("ANY"));

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
        "src", GST_PAD_SRC, GST_PAD_ALWAYS,
        GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE(
                "{ I420, RGB, BGR, RGBx, xRGB, BGRx, xBGR, GRAY8 }")));

#define gst_evtest_parent_class parent_class
G_DEFINE_TYPE(GstEvTest, gst_evtest, GST_TYPE_ELEMENT);

static void gst_evtest_set_property(GObject* object, guint prop_id,
                                    const GValue* value, GParamSpec* pspec);
static void gst_evtest_get_property(GObject* object, guint prop_id,
                                    GValue* value, GParamSpec* pspec);

static gboolean gst_evtest_sink_event(GstPad* pad, GstObject* parent,
                                      GstEvent* event);
static gboolean gst_evtest_sink_query(GstPad* pad, GstObject* parent,
                                      GstQuery* query);
static GstFlowReturn gst_evtest_chain(GstPad* pad, GstObject* parent,
                                      GstBuffer* buf);
static gboolean gst_evtest_src_event(GstPad* pad, GstObject* parent,
                                     GstEvent* event);
static gboolean gst_evtest_src_query(GstPad* pad, GstObject* parent,
                                     GstQuery* query);
static gboolean gst_evtest_send_event(GstElement* element, GstEvent* event);

/* GObject vmethod implementations */

/* initialize the plugin's class */
static void gst_evtest_class_init(GstEvTestClass* klass)
{
	GObjectClass* gobject_class;
	GstElementClass* gstelement_class;

	GST_DEBUG_CATEGORY_INIT(gst_evtest_debug, "evtest", 0, "EvTest Plugin");

	gobject_class = (GObjectClass*)klass;
	gstelement_class = (GstElementClass*)klass;

	gobject_class->set_property = gst_evtest_set_property;
	gobject_class->get_property = gst_evtest_get_property;

	g_object_class_install_property(
	        gobject_class, PROP_SILENT,
	        g_param_spec_boolean("silent", "Silent",
	                             "Produce verbose output ?", FALSE,
	                             G_PARAM_READWRITE));

	gst_element_class_set_details_simple(
	        gstelement_class, "Event Test", "Generic",
	        "Generic Template Element", "Varphone Wong <varphone@qq.com>");

	gst_element_class_add_pad_template(
	        gstelement_class, gst_static_pad_template_get(&src_factory));
	gst_element_class_add_pad_template(
	        gstelement_class, gst_static_pad_template_get(&sink_factory));
	gstelement_class->send_event = gst_evtest_send_event;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void gst_evtest_init(GstEvTest* filter)
{
	filter->sinkpad =
	        gst_pad_new_from_static_template(&sink_factory, "sink");
	gst_pad_set_event_function(filter->sinkpad,
	                           GST_DEBUG_FUNCPTR(gst_evtest_sink_event));
	gst_pad_set_query_function(filter->sinkpad,
	                           GST_DEBUG_FUNCPTR(gst_evtest_sink_query));
	gst_pad_set_chain_function(filter->sinkpad,
	                           GST_DEBUG_FUNCPTR(gst_evtest_chain));
	// GST_PAD_SET_PROXY_CAPS(filter->sinkpad);
	gst_element_add_pad(GST_ELEMENT(filter), filter->sinkpad);

	filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
	gst_pad_set_event_function(filter->srcpad,
	                           GST_DEBUG_FUNCPTR(gst_evtest_src_event));
	gst_pad_set_query_function(filter->srcpad,
	                           GST_DEBUG_FUNCPTR(gst_evtest_src_query));
	// GST_PAD_SET_PROXY_CAPS(filter->srcpad);
	gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

	filter->silent = FALSE;
}

static void gst_evtest_set_property(GObject* object, guint prop_id,
                                    const GValue* value, GParamSpec* pspec)
{
	GstEvTest* filter = GST_EVTEST(object);

	switch (prop_id) {
	case PROP_SILENT:
		filter->silent = g_value_get_boolean(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void gst_evtest_get_property(GObject* object, guint prop_id,
                                    GValue* value, GParamSpec* pspec)
{
	GstEvTest* filter = GST_EVTEST(object);

	switch (prop_id) {
	case PROP_SILENT:
		g_value_set_boolean(value, filter->silent);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean gst_evtest_sink_event(GstPad* pad, GstObject* parent,
                                      GstEvent* event)
{
	GstEvTest* filter;
	gboolean ret;

	filter = GST_EVTEST(parent);

	GST_LOG_OBJECT(pad, "event = %" GST_PTR_FORMAT, event);

	switch (GST_EVENT_TYPE(event)) {
	case GST_EVENT_CAPS: {
		GstCaps* caps = NULL;

		gst_event_parse_caps(event, &caps);
		/* do something with the caps */
		if (caps) {
			GST_LOG_OBJECT(pad, "caps = %" GST_PTR_FORMAT, caps);
		}
		/* and forward */
		ret = gst_pad_event_default(pad, parent, event);
		break;
	}
	default:
		ret = gst_pad_event_default(pad, parent, event);
		break;
	}
	return ret;
}

static gboolean gst_evtest_sink_query(GstPad* pad, GstObject* parent,
                                      GstQuery* query)
{
	gboolean ret = FALSE;
	GST_LOG_OBJECT(pad, "query = %" GST_PTR_FORMAT, query);
	switch (GST_QUERY_TYPE(query)) {
	case GST_QUERY_ACCEPT_CAPS: {
		GstCaps* caps;
		gst_query_parse_accept_caps(query, &caps);
		gst_query_set_accept_caps_result(query, TRUE);
		ret = TRUE;
		break;
	}
	case GST_QUERY_CAPS: {
		GstCaps* filter;
		gst_query_parse_caps(query, &filter);
		GstCaps* caps = gst_caps_new_simple(
		        "video/x-raw", "format", G_TYPE_STRING, "BGRx",
		        "framerate", GST_TYPE_FRACTION, 25, 1,
		        "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1, "width",
		        G_TYPE_INT, 640, "height", G_TYPE_INT, 480,
		        "interlace-mode", G_TYPE_STRING, "progressive", NULL);
		if (filter) {
			GstCaps* filtered;
			filtered = gst_caps_intersect_full(
			        filter, caps, GST_CAPS_INTERSECT_FIRST);
			gst_caps_unref(caps);
			caps = filtered;
		}
		gst_query_set_caps_result(query, caps);
		gst_caps_unref(caps);
		ret = TRUE;
		break;
	}
	default:
		ret = gst_pad_query_default(pad, parent, query);
		break;
	}
	GST_LOG_OBJECT(pad, "final query = %" GST_PTR_FORMAT, query);
	return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn gst_evtest_chain(GstPad* pad, GstObject* parent,
                                      GstBuffer* buf)
{
	GstEvTest* filter;

	filter = GST_EVTEST(parent);

	if (filter->silent == FALSE)
		g_print("I'm plugged, therefore I'm in.\n");

	gst_buffer_unref(buf);
	return GST_FLOW_OK;
	/* just push out the incoming buffer without touching it */
	return gst_pad_push(filter->srcpad, buf);
}

static gboolean gst_evtest_src_event(GstPad* pad, GstObject* parent,
                                     GstEvent* event)
{
	gboolean ret = FALSE;

	GST_LOG_OBJECT(pad, "event = %" GST_PTR_FORMAT, event);

	switch (GST_EVENT_TYPE(event)) {
	case GST_EVENT_CAPS: {
		GstCaps* caps;
		gst_event_parse_caps(event, &caps);
		if (caps) {
			GST_LOG_OBJECT(pad, "caps = %" GST_PTR_FORMAT, caps);
			ret = TRUE;
		}
		break;
	}
	default:
		break;
	}
	return ret;
}

static gboolean gst_evtest_src_query(GstPad* pad, GstObject* parent,
                                     GstQuery* query)
{
	gboolean ret = FALSE;
	GST_LOG_OBJECT(pad, "query = %" GST_PTR_FORMAT, query);
	switch (GST_QUERY_TYPE(query)) {
	case GST_QUERY_ACCEPT_CAPS: {
		GstCaps* caps;
		gst_query_parse_accept_caps(query, &caps);
		gst_query_set_accept_caps_result(query, TRUE);
		ret = TRUE;
		break;
	}
	case GST_QUERY_CAPS: {
		GstCaps* filter;
		gst_query_parse_caps(query, &filter);
		GstCaps* caps = gst_caps_new_simple(
		        "video/x-raw", "format", G_TYPE_STRING, "BGRx",
		        "framerate", GST_TYPE_FRACTION, 25, 1,
		        "pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1, "width",
		        G_TYPE_INT, 1280, "height", G_TYPE_INT, 720,
		        "interlace-mode", G_TYPE_STRING, "progressive", NULL);
		if (filter) {
			GstCaps* filtered;
			filtered = gst_caps_intersect_full(
			        filter, caps, GST_CAPS_INTERSECT_FIRST);
			gst_caps_unref(caps);
			caps = filtered;
		}
		gst_query_set_caps_result(query, caps);
		gst_caps_unref(caps);
		ret = TRUE;
		break;
	}
	default:
		ret = gst_pad_query_default(pad, parent, query);
		break;
	}
	GST_LOG_OBJECT(pad, "final query = %" GST_PTR_FORMAT, query);
	return ret;
}

static gboolean gst_evtest_send_event(GstElement* element, GstEvent* event)
{
	GST_LOG_OBJECT(element, "event = %" GST_PTR_FORMAT, event);

	return FALSE;
}