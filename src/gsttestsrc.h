#ifndef __GST_TESTSRC_H__
#define __GST_TESTSRC_H__

#include <gst/gst.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_TESTSRC (gst_testsrc_get_type())
#define GST_TESTSRC(obj)                                                       \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_TESTSRC, GstTestSrc))
#define GST_TESTSRC_CLASS(klass)                                               \
	(G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_TESTSRC, GstTestSrcClass))
#define GST_TESTSRC_GET_CLASS(obj)                                             \
	(G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_TESTSRC, GstTestSrcClass))
#define GST_IS_TESTSRC(obj)                                                    \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_TESTSRC))
#define GST_IS_TESTSRC_CLASS(klass)                                            \
	(G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_TESTSRC))

typedef enum {
	GST_TESTSRC_FLAG_STARTING = (GST_ELEMENT_FLAG_LAST << 0),
	GST_TESTSRC_FLAG_STARTED = (GST_ELEMENT_FLAG_LAST << 1),
	/* padding */
	GST_TESTSRC_FLAG_LAST = (GST_ELEMENT_FLAG_LAST << 6)
} GstTestSrcFlags;

#define GST_TESTSRC_IS_STARTING(obj)                                           \
	GST_OBJECT_FLAG_IS_SET((obj), GST_TESTSRC_FLAG_STARTING)
#define GST_TESTSRC_IS_STARTED(obj)                                            \
	GST_OBJECT_FLAG_IS_SET((obj), GST_TESTSRC_FLAG_STARTED)

typedef struct _GstTestSrc GstTestSrc;
typedef struct _GstTestSrcPrivate GstTestSrcPrivate;
typedef struct _GstTestSrcClass GstTestSrcClass;

struct _GstTestSrc
{
	GstElement element;

	GstPad* srcpad;

	GstClockID clock_id; /* for syncing */

	/* MT-protected (with STREAM_LOCK *and* OBJECT_LOCK) */
	GstSegment segment;
	/* MT-protected (with STREAM_LOCK) */
	gboolean need_newsegment;

	gint num_buffers;
	gint num_buffers_left;

	gboolean running;
	GstEvent* pending_seek;
	gboolean silent;

	GstTestSrcPrivate* priv;

	/*< private >*/
	gpointer _gst_reserved[GST_PADDING_LARGE];
};

struct _GstTestSrcClass
{
	GstElementClass parent_class;
};

GType gst_testsrc_get_type(void);

G_END_DECLS

#endif /* __GST_TESTSRC_H__ */
