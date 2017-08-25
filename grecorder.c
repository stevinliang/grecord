/*********************************************************************************************
 * Author: Stevin Liang
 * Version: v0.1
 *
 *********************************************************************************************/
#include <gst/gst.h>
#include <unistd.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

/* grecord struct manage all the element and pipeline */
struct grecorder {
	GstElement *pipeline;
	GstElement *camera_dev;
	GstElement *vpe;
	GstElement *vpe_queue;
	GstElement *encode;
	GstElement *encode_queue;
	GstElement *appsink;
	GMainLoop  *loop;
};

/**
 * @function: usage
 * @parm command: command name of this program.
 * @return: none.
 * @description:
 *           printf help message for this program
 **/
static void usage(char *command)
{
	g_print(("Usage: %s -D device -W width -H height -F fourcc -O FILE ...\n"
				"\n"
				"       -h, --help              Help\n"
				"       -D, --device=NAME       Select Video Device\n"
				"       -W, --width=Width       Picture Width\n"
				"       -H, --height=Height     Picture Height\n"
				"       -F, --fourcc=FORMAT     Camera Fourcc code\n"
				"       -B, --bitrate=rate      Encoding bitrate\n"
				"       -O, --output=file       Output file\n"
		), command);
}

static struct grecorder recorder;

/**
 * @function: intr_handler
 * @parm signum: signal number
 * @return: none
 * @description:
 *         signal callback function for SIGINT
 **/
static void intr_handler (int signum)
{
	GstElement *pipeline = recorder.pipeline;

	g_print ("handling interrupt.\n");
	gst_element_send_event (pipeline, gst_event_new_eos ());

	/* post an application specific message */
	gst_element_post_message (GST_ELEMENT(pipeline),
			gst_message_new_application (GST_OBJECT (pipeline),
			gst_structure_new ("GstLaunchInterrupt", "message",
			G_TYPE_STRING, "Pipeline interrupted", NULL)));
}

static GstFlowReturn on_new_sample_from_sink(GstElement *sink_elm, struct grecorder *data)
{
	GstSample *sample = NULL;
	GstBuffer *buffer, *app_buffer;
	GstElement *sink, *source;
	GstFlowReturn ret;
	GstMapInfo map;

	/* get the sample from appsink */
	//sample = gst_app_sink_pull_sample (GST_APP_SINK (sink_elm));
#if 0
	sink = gst_bin_get_by_name (GST_BIN (data->src_pipeline), "testsink");
	g_signal_emit_by_name (sink, "pull-sample", &sample, &ret);
	gst_object_unref (sink);
#else
	g_signal_emit_by_name(sink_elm, "pull-sample", &sample, &ret);
#endif

	if(sample){
		buffer = gst_sample_get_buffer (sample);
		g_print("on_new_sample_from_sink() call!; size = %d\n", gst_buffer_get_size(buffer));
	}
	else{
		g_print("sample is NULL \n");
		return ret;
	}


	/* pull sample data */
#if 1
	/* Mapping a buffer can fail (non-readable) */
	if (gst_buffer_map (buffer, &map, GST_MAP_READ)) {
		/* print the buffer data for debug */
		int i = 0, j = 0;
		for(; i < 10; i++)
			g_print("%x ", map.data[i]);
		g_print("\n");
	}
#endif

	/* we don't need the appsink sample anymore */
	gst_sample_unref (sample);

	return ret;

}
static gboolean on_appsink_message (GstBus * bus, GstMessage * msg, struct grecorder *data)
{
	GstElement *source;
	GError *err;
	gchar *debug_info;

	switch (GST_MESSAGE_TYPE (msg)) {
	case GST_MESSAGE_EOS:
		g_print ("The source got dry\n");
		source = gst_bin_get_by_name (GST_BIN (data->pipeline), "app_sink");
		//gst_app_src_end_of_stream (GST_APP_SRC (source));
		g_signal_emit_by_name (source, "end-of-stream", NULL);
		gst_object_unref (source);
		g_print ("End-Of-Stream reached.\n");
		break;

	case GST_MESSAGE_ERROR:
		gst_message_parse_error (msg, &err, &debug_info);
		g_printerr ("Error received from element %s: %s\n",
				GST_OBJECT_NAME (msg->src), err->message);
		g_printerr ("Debugging information: %s\n",
				debug_info ? debug_info : "none");
		g_main_loop_quit (data->loop);
		g_clear_error (&err);
		g_free (debug_info);
		break;

	case GST_MESSAGE_NEW_CLOCK:
		g_print ("New Clock Detect.\n");
		break;

	case GST_MESSAGE_APPLICATION:
		{
			const GstStructure *s;
			s = gst_message_get_structure(msg);
			if (gst_structure_has_name(s, "GstLaunchInterrupt")) {
				g_print("Interrupt: Stopping pipeline ...\n");
				g_main_loop_quit (data->loop);
			}
		}
		break;

	default:
		/* We should not reach here because we only asked for ERRORs and EOS */
		g_printerr ("Unexpected message received, message type name %s.\n",
				GST_MESSAGE_TYPE_NAME(msg));
		break;

	}
	return TRUE;
}

int main (int argc, char **argv)
{
	int width = 0;
	int height = 0;
	int bitrate = 0;
	int opt_idx;
	char *camera = NULL;
	char *command = NULL;
	char *fourcc = NULL;
	char *output = NULL;
	static const char short_options[] = {"hD:W:H:F:B:O:v"};
	static const struct option long_options[] = {
		{"help", 0, 0, 'h'},
		{"device", 1, 0, 'D'},
		{"width", 1, 0, 'W'},
		{"height", 1, 0, 'H'},
		{"fourcc", 1, 0, 'F'},
		{"bitrate", 1, 0, 'B'},
		{"output", 1, 0, 'O'},
		{"verbose", 0, 0, 'v'},
		{0, 0, 0, 0}
	};
	int err, c;
	GstCaps *raw_video_caps = NULL;
	GstBus *bus = NULL;
	GstMessage *msg = NULL;
	GstElement *appsink = NULL;

	command = argv[0];

	/* Parse command line args */
	while ((c = getopt_long(argc, argv, short_options, long_options, &opt_idx)) != -1) {
		switch (c) {
		case 'h':
			usage(command);
			break;
		case 'D':
			camera = optarg;
			break;
		case 'W':
			width = atoi(optarg);
			break;
		case 'H':
			height = atoi(optarg);
			break;
		case 'F':
			fourcc = optarg;
			break;
		case 'B':
			bitrate = atoi(optarg);
			break;
		case 'O':
			output = optarg;
			break;
		case 'v':
			break;
		default:
			g_printerr("Try `%s --help` for more information.\n", command);
			return 1;

		}
	}

	if (width <= 0 || height <= 0 || bitrate <= 0|| !fourcc || !camera) {
		usage(command);
		return -1;
	}

	g_print("camera:%s resolution %dx%d, format=%s, bitrate=%dkbps\n",
			camera, width, height, fourcc, bitrate);

	/* Init Gstreamer */
	gst_init(&argc, &argv);

	recorder.pipeline = gst_pipeline_new("grecord-pipeline");
	if (!recorder.pipeline) {
		g_printerr("No resource to create pipeline.\n");
		return -1;
	}

	recorder.loop = g_main_loop_new(NULL, FALSE);
	recorder.camera_dev = gst_element_factory_make("v4l2src", "video_source");
	recorder.vpe = gst_element_factory_make("vpe","ti_vpe");
	recorder.vpe_queue = gst_element_factory_make("queue", "vpe_queue");
	recorder.encode = gst_element_factory_make("ducatih264enc", "h264encoder");
	recorder.encode_queue = gst_element_factory_make("queue", "encode_queue");
	recorder.appsink = gst_element_factory_make("appsink", "app_sink");

	if (!recorder.camera_dev || !recorder.vpe || !recorder.vpe_queue || !recorder.encode ||
			!recorder.encode_queue || !recorder.appsink) {
		g_printerr("No resource to create one element.\n");
		return -1;
	}

	gst_bin_add_many(GST_BIN(recorder.pipeline), recorder.camera_dev, recorder.vpe,
			recorder.vpe_queue, recorder.encode, recorder.encode_queue,
			recorder.appsink, NULL);

	/* Setup camera_dev element property */
	g_object_set(G_OBJECT(recorder.camera_dev), "device", camera, "io-mode", 4, NULL);
	/* Setup vpe property */
	g_object_set(G_OBJECT(recorder.vpe), "num-input-buffers", 8, NULL);
	g_object_set(G_OBJECT(recorder.encode), "bitrate", bitrate, NULL);
	//g_object_set(G_OBJECT(recorder.filesink), "location", output, NULL);
	/* cap filter between camera and vpe */
	raw_video_caps = gst_caps_new_simple("video/x-raw",
			"format", G_TYPE_STRING, fourcc,
			"width", G_TYPE_INT, width,
			"height", G_TYPE_INT, height,
			"framerate", GST_TYPE_FRACTION, 30, 1,
			NULL);
	if (!raw_video_caps) {
		g_printerr("No resource to create one element.\n");
		return -1;
	}

	/* Link camera_dev and vpe */
	if (!gst_element_link_filtered (recorder.camera_dev, recorder.vpe, raw_video_caps)) {
		g_printerr("Failed to link camera and vpe.\n");
		return -1;
	}

	gst_element_link_many(recorder.vpe, recorder.vpe_queue, recorder.encode,
			recorder.encode_queue, recorder.appsink, NULL);

	bus = gst_element_get_bus(recorder.pipeline);
	gst_bus_add_watch(bus, (GstBusFunc) on_appsink_message, &recorder);
	gst_object_unref(bus);


	appsink = gst_bin_get_by_name(GST_BIN(recorder.pipeline), "app_sink");
	g_object_set(G_OBJECT(appsink), "emit-signals", TRUE, "sync", FALSE, NULL);
	g_signal_connect(appsink, "new-sample", G_CALLBACK(on_new_sample_from_sink), &recorder);
	gst_object_unref(appsink);

	err = gst_element_set_state(recorder.pipeline, GST_STATE_PLAYING);
	if (err == GST_STATE_CHANGE_FAILURE) {
		g_printerr ("Unable to set the pipeline to the playing state.\n");
		g_object_unref(recorder.pipeline);
		return -1;
	}

	signal(SIGINT, intr_handler);
	g_main_loop_run(recorder.loop);

	gst_element_set_state(recorder.pipeline, GST_STATE_PAUSED);
	gst_element_set_state(recorder.pipeline, GST_STATE_READY);
	gst_element_set_state(recorder.pipeline, GST_STATE_NULL);
	g_object_unref(recorder.pipeline);
	gst_deinit();

	return 0;
}
