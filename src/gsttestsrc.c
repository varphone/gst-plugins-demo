#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>
#include <gst/video/video.h>

#include "gsttestsrc.h"

GST_DEBUG_CATEGORY_STATIC(gst_testsrc_debug);
#define GST_CAT_DEFAULT gst_testsrc_debug

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

#define DEFAULT_BLOCKSIZE 4096
#define DEFAULT_NUM_BUFFERS -1
#define DEFAULT_DO_TIMESTAMP FALSE

struct _GstTestSrcPrivate
{
	gboolean discont;  /* STREAM_LOCK */
	gboolean flushing; /* LIVE_LOCK */

	GstFlowReturn start_result; /* OBJECT_LOCK */
	gboolean async;             /* OBJECT_LOCK */

	/* if a stream-start event should be sent */
	gboolean stream_start_pending; /* STREAM_LOCK */

	/* if segment should be sent and a
	 * seqnum if it was originated by a seek */
	gboolean segment_pending; /* OBJECT_LOCK */
	guint32 segment_seqnum;   /* OBJECT_LOCK */

	/* if EOS is pending (atomic) */
	GstEvent* pending_eos; /* OBJECT_LOCK */
	gint has_pending_eos;  /* atomic */

	/* if the eos was caused by a forced eos from the application */
	gboolean forced_eos; /* LIVE_LOCK */

	/* startup latency is the time it takes between going to PLAYING and
	 * producing
	 * the first BUFFER with running_time 0. This value is included in the
	 * latency
	 * reporting. */
	GstClockTime latency; /* OBJECT_LOCK */
	/* timestamp offset, this is the offset add to the values of gst_times
	 * for
	 * pseudo live sources */
	GstClockTimeDiff ts_offset; /* OBJECT_LOCK */

	gboolean do_timestamp;       /* OBJECT_LOCK */
	volatile gint dynamic_size;  /* atomic */
	volatile gint automatic_eos; /* atomic */

	/* stream sequence number */
	guint32 seqnum; /* STREAM_LOCK */

	/* pending events (TAG, CUSTOM_BOTH, CUSTOM_DOWNSTREAM) to be
	 * pushed in the data stream */
	GList* pending_events;     /* OBJECT_LOCK */
	volatile gint have_events; /* OBJECT_LOCK */

	/* QoS */                   /* with LOCK */
	gdouble proportion;         /* OBJECT_LOCK */
	GstClockTime earliest_time; /* OBJECT_LOCK */

	GstBufferPool* pool;        /* OBJECT_LOCK */
	GstAllocator* allocator;    /* OBJECT_LOCK */
	GstAllocationParams params; /* OBJECT_LOCK */

	GCond async_cond; /* OBJECT_LOCK */
};

#define GST_TESTSRC_GET_PRIVATE(obj)                                           \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), GST_TYPE_TESTSRC,                  \
	                             GstTestSrcPrivate))

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE(
        "src", GST_PAD_SRC, GST_PAD_ALWAYS,
        GST_STATIC_CAPS(GST_VIDEO_CAPS_MAKE(
                "{ I420, RGB, BGR, RGBx, xRGB, BGRx, xBGR, GRAY8 }")));

#define gst_testsrc_parent_class parent_class
G_DEFINE_TYPE(GstTestSrc, gst_testsrc, GST_TYPE_ELEMENT);

static void gst_testsrc_set_property(GObject* object, guint prop_id,
                                     const GValue* value, GParamSpec* pspec);
static void gst_testsrc_get_property(GObject* object, guint prop_id,
                                     GValue* value, GParamSpec* pspec);
static gboolean gst_testsrc_src_activate_mode(GstPad* pad, GstObject* parent,
                                              GstPadMode mode, gboolean active);
static gboolean gst_testsrc_src_event(GstPad* pad, GstObject* parent,
                                      GstEvent* event);
static gboolean gst_testsrc_src_query(GstPad* pad, GstObject* parent,
                                      GstQuery* query);
static GstStateChangeReturn gst_testsrc_change_state(GstElement* element,
                                                     GstStateChange transition);
static gboolean gst_testsrc_send_event(GstElement* element, GstEvent* event);

/* GObject vmethod implementations */

/* initialize the plugin's class */
static void gst_testsrc_class_init(GstTestSrcClass* klass)
{
	GObjectClass* gobject_class;
	GstElementClass* gstelement_class;

	GST_DEBUG_CATEGORY_INIT(gst_testsrc_debug, "testsrc", 0,
	                        "TestSrc Plugin");

	g_type_class_add_private(klass, sizeof(GstTestSrcPrivate));
	//	parent_class = g_type_class_peek_parent(klass);

	gobject_class = (GObjectClass*)klass;
	gstelement_class = (GstElementClass*)klass;

	gobject_class->set_property = gst_testsrc_set_property;
	gobject_class->get_property = gst_testsrc_get_property;

	g_object_class_install_property(
	        gobject_class, PROP_SILENT,
	        g_param_spec_boolean("silent", "Silent",
	                             "Produce verbose output ?", FALSE,
	                             G_PARAM_READWRITE));

	gst_element_class_set_details_simple(
	        gstelement_class, "Source Test", "Generic",
	        "Generic Template Element", "Varphone Wong <varphone@qq.com>");

	gst_element_class_add_pad_template(
	        gstelement_class, gst_static_pad_template_get(&src_factory));
	gstelement_class->change_state = gst_testsrc_change_state;
	gstelement_class->send_event = gst_testsrc_send_event;
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void gst_testsrc_init(GstTestSrc* filter)
{
	filter->priv = GST_TESTSRC_GET_PRIVATE(filter);
	filter->num_buffers = DEFAULT_NUM_BUFFERS;
	filter->num_buffers_left = 0;
	filter->clock_id = NULL;
	filter->silent = FALSE;
	filter->priv->do_timestamp = DEFAULT_DO_TIMESTAMP;
	filter->priv->start_result = GST_FLOW_FLUSHING;
	filter->srcpad = gst_pad_new_from_static_template(&src_factory, "src");
	gst_pad_set_activatemode_function(
	        filter->srcpad,
	        GST_DEBUG_FUNCPTR(gst_testsrc_src_activate_mode));
	gst_pad_set_event_function(filter->srcpad,
	                           GST_DEBUG_FUNCPTR(gst_testsrc_src_event));
	gst_pad_set_query_function(filter->srcpad,
	                           GST_DEBUG_FUNCPTR(gst_testsrc_src_query));
	gst_element_add_pad(GST_ELEMENT(filter), filter->srcpad);

	GST_OBJECT_FLAG_UNSET(filter, GST_TESTSRC_FLAG_STARTED);
	GST_OBJECT_FLAG_UNSET(filter, GST_TESTSRC_FLAG_STARTING);
	GST_OBJECT_FLAG_SET(filter, GST_ELEMENT_FLAG_SOURCE);
}

static void gst_testsrc_set_property(GObject* object, guint prop_id,
                                     const GValue* value, GParamSpec* pspec)
{
	GstTestSrc* filter = GST_TESTSRC(object);

	switch (prop_id) {
	case PROP_SILENT:
		filter->silent = g_value_get_boolean(value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static void gst_testsrc_get_property(GObject* object, guint prop_id,
                                     GValue* value, GParamSpec* pspec)
{
	GstTestSrc* filter = GST_TESTSRC(object);

	switch (prop_id) {
	case PROP_SILENT:
		g_value_set_boolean(value, filter->silent);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
		break;
	}
}

static GstCaps* gst_testsrc_get_caps(GstTestSrc* src)
{
	GstCaps* caps;
	/* clang-format off */
	caps = gst_caps_new_simple("video/x-raw",
			"format", G_TYPE_STRING, "BGRx",
			"framerate", GST_TYPE_FRACTION, 25, 1,
			"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
			"width", G_TYPE_INT, 1280,
			"height", G_TYPE_INT, 720,
			"interlace-mode", G_TYPE_STRING, "progressive",
			NULL);
	/* clang-format on */
	return caps;
}

/* GstElement vmethod implementations */
static gboolean gst_testsrc_src_send_stream_start(GstTestSrc* src)
{
	gboolean ret = TRUE;

	if (src->priv->stream_start_pending) {
		gchar* stream_id;
		GstEvent* event;

		stream_id = gst_pad_create_stream_id(
		        src->srcpad, GST_ELEMENT_CAST(src), NULL);

		GST_DEBUG_OBJECT(src, "Pushing STREAM_START");
		event = gst_event_new_stream_start(stream_id);
		gst_event_set_group_id(event, gst_util_group_id_next());
		ret = gst_pad_push_event(src->srcpad, event);

		event = gst_event_new_caps(gst_testsrc_get_caps(src));
		ret = gst_pad_push_event(src->srcpad, event);

		src->priv->stream_start_pending = FALSE;
		g_free(stream_id);
	}

	return ret;
}

static void gst_testsrc_src_loop(GstPad* pad)
{
	GstTestSrc* src;

	src = GST_TESTSRC(GST_PAD_PARENT(pad));

	GST_DEBUG_OBJECT(pad, "task loop");

	if (GST_PAD_IS_EOS(pad)) {
		GST_DEBUG_OBJECT(src, "Pad is marked as EOS, pause the task");
		gst_pad_pause_task(pad);
		return;
	}

	gst_testsrc_src_send_stream_start(src);
}

static gboolean gst_testsrc_push_frame(GstClock* clock, GstClockTime time,
                                       GstClockID id, gpointer user_data)
{
	GstTestSrc* src = (GstTestSrc*)user_data;
	GST_DEBUG_OBJECT(src, "push frame");
	GstBuffer* buf = gst_buffer_new();
	gst_pad_push(src->srcpad, buf);
	// gst_clock_id_wait_async(src->clock_id, gst_testsrc_push_frame, src,
	// NULL);
	return TRUE;
}

static gboolean gst_testsrc_set_playing(GstTestSrc* src, gboolean playing)
{
	GST_LOG_OBJECT(src, "set_playing %d", playing);

	if (playing) {
#if 0
		GstClock* clk = GST_ELEMENT_CLOCK(src);
		GstClockTime start = gst_clock_get_time(clk);
		src->clock_id =
		        gst_clock_new_periodic_id(clk, start, GST_SECOND / 25);
		gst_clock_id_wait_async(src->clock_id, gst_testsrc_push_frame,
		                        src, NULL);
#else
		gst_pad_start_task(src->srcpad,
		                   (GstTaskFunction)gst_testsrc_src_loop,
		                   src->srcpad, NULL);
#endif
	}
	else {
		gst_pad_stop_task(src->srcpad);
	}
	return TRUE;
}

static gboolean gst_testsrc_start(GstTestSrc* src)
{
	GstTestSrcClass* klass = GST_TESTSRC_GET_CLASS(src);

	GST_OBJECT_LOCK(src);
	src->priv->start_result = GST_FLOW_FLUSHING;
	GST_OBJECT_FLAG_SET(src, GST_TESTSRC_FLAG_STARTING);
	gst_segment_init(&src->segment, src->segment.format);
	GST_OBJECT_UNLOCK(src);
	src->num_buffers_left = src->num_buffers;
	src->running = FALSE;
	src->priv->segment_pending = FALSE;
	src->priv->segment_seqnum = gst_util_seqnum_next();
	src->priv->forced_eos = FALSE;
	return TRUE;
}

static gboolean gst_testsrc_stop(GstTestSrc* src)
{
	GST_OBJECT_FLAG_UNSET(src, GST_TESTSRC_FLAG_STARTING);
	GST_OBJECT_FLAG_UNSET(src, GST_TESTSRC_FLAG_STARTED);
	// gst_pad_stop_task(src->srcpad);
	return TRUE;
}

static gboolean gst_testsrc_src_activate_push(GstPad* pad, GstObject* parent,
                                              gboolean active)
{
	GstTestSrc* src = GST_TESTSRC(parent);
	if (active)
		return gst_testsrc_start(src);
	else
		return gst_testsrc_stop(src);
}

static gboolean gst_testsrc_src_activate_mode(GstPad* pad, GstObject* parent,
                                              GstPadMode mode, gboolean active)
{

	gboolean res;
	GstTestSrc* src = GST_TESTSRC(parent);

	src->priv->stream_start_pending = FALSE;

	GST_DEBUG_OBJECT(pad, "activating in mode %d, %d", mode, active);

	switch (mode) {
	case GST_PAD_MODE_PULL:
		res = FALSE;
		break;
	case GST_PAD_MODE_PUSH:
		src->priv->stream_start_pending = active;
		res = gst_testsrc_src_activate_push(pad, parent, active);
		break;
	default:
		GST_LOG_OBJECT(pad, "unknown activation mode %d", mode);
		res = FALSE;
		break;
	}
	return active ? res : TRUE;
}

static gboolean gst_testsrc_src_event(GstPad* pad, GstObject* parent,
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
	case GST_EVENT_EOS: {
		ret = TRUE;
		break;
	}
	case GST_EVENT_RECONFIGURE: {
		ret = TRUE;
		break;
	}
	default:
		ret = gst_pad_event_default(pad, parent, event);
		break;
	}
	return ret;
}

static gboolean gst_testsrc_src_query(GstPad* pad, GstObject* parent,
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

static GstStateChangeReturn gst_testsrc_change_state(GstElement* element,
                                                     GstStateChange transition)
{
	GstTestSrc* src;
	GstTestSrcClass* src_class;
	GstStateChangeReturn result;
	gboolean no_preroll = FALSE;

	src = GST_TESTSRC(element);
	src_class = GST_TESTSRC_GET_CLASS(src);

	GST_LOG_OBJECT(element, "transition = %d", transition);

	switch (transition) {
	case GST_STATE_CHANGE_NULL_TO_READY:
		GST_LOG("NULL -> READY");
		break;
	case GST_STATE_CHANGE_READY_TO_PAUSED:
		GST_LOG("READY -> PAUSED");
		no_preroll = TRUE;
		break;
	case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
		GST_DEBUG_OBJECT(src, "PAUSED -> PLAYING");
		gst_testsrc_set_playing(src, TRUE);
		break;
	default:
		break;
	}

	result = GST_ELEMENT_CLASS(parent_class)
	                 ->change_state(element, transition);
	if (result == GST_STATE_CHANGE_FAILURE) {
		GST_LOG("change state failed: %d", result);
		return result;
	}

	switch (transition) {
	case GST_STATE_CHANGE_PLAYING_TO_PAUSED:
		GST_DEBUG_OBJECT(src, "PLAYING -> PAUSED");
		gst_testsrc_set_playing(src, FALSE);
		no_preroll = FALSE; // TRUE;
		break;
	case GST_STATE_CHANGE_PAUSED_TO_READY: {
		GST_LOG("PAUSE -> READY");
		break;
	}
	case GST_STATE_CHANGE_READY_TO_NULL:
		GST_LOG("READY -> NULL");
		break;
	default:
		break;
	}

	if (no_preroll && result == GST_STATE_CHANGE_SUCCESS)
		result = GST_STATE_CHANGE_NO_PREROLL;

	GST_LOG_OBJECT(element, "final transition = %d", transition);

	return result;
}

static gboolean gst_testsrc_send_event(GstElement* element, GstEvent* event)
{
	GST_LOG_OBJECT(element, "event = %" GST_PTR_FORMAT, event);

	return FALSE;
}