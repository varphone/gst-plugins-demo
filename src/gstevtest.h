#ifndef __GST_EVTEST_H__
#define __GST_EVTEST_H__

#include <gst/gst.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_EVTEST \
	(gst_evtest_get_type())
#define GST_EVTEST(obj) \
	(G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_EVTEST, GstEvTest))
#define GST_EVTEST_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_EVTEST, GstEvTestClass))
#define GST_IS_EVTEST(obj) \
	(G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_EVTEST))
#define GST_IS_EVTEST_CLASS(klass) \
	(G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_EVTEST))

typedef struct _GstEvTest GstEvTest;
typedef struct _GstEvTestClass GstEvTestClass;

struct _GstEvTest
{
	GstElement element;

	GstPad *sinkpad, *srcpad;

	gboolean silent;
};

struct _GstEvTestClass
{
	GstElementClass parent_class;
};

GType gst_evtest_get_type(void);

G_END_DECLS

#endif /* __GST_EVTEST_H__ */
