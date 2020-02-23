
#include <rtvc/pipeline/source.hpp>
#include <rtvc/pipeline/visualization.hpp>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#include <stdio.h>
#include <iostream>

#include <boost/program_options.hpp>


/* This function is called when an error message is posted on the bus */
template <typename F>
static void error_cb (GstBus *bus, GstMessage *msg, void *data)
{
  (*static_cast<F*>(data)) (bus, msg);
}

int
main (int   argc,
      char *argv[])
{
  guint major, minor, micro, nano;

  std::string host1, host2, user, password;
  int port1, port2;
  
  {
    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
      ("help", "produce help message")
      ("host", po::value<std::string>(), "Connection to NVR")
      ("port", po::value<int>(), "Port for connection to NVR")
      ("failover-host", po::value<std::string>(), "Connection failover to NVR")
      ("failover-port", po::value<int>(), "Port for connection failover to NVR")
      ("user", po::value<std::string>(), "User for connection to NVR")
      ("pass", po::value<std::string>(), "Password for connection to NVR")
      ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    

    if (vm.count("help")
        || !vm.count("host")
        || !vm.count("port")
        || !vm.count("user")
        || !vm.count("pass"))
    {
      std::cout << desc << "\n";
      return 1;
    }

    host1 = vm["host"].as<std::string>();
    user = vm["user"].as<std::string>();
    password = vm["pass"].as<std::string>();
    port1 = vm["port"].as<int>();

    if (vm.count("compression")) {
      std::cout << "Compression level was set to " 
           << vm["compression"].as<int>() << ".\n";
    } else {
      std::cout << "Compression level was not set.\n";
    }
  }
  
  gst_init (&argc, &argv);

  gst_version (&major, &minor, &micro, &nano);

  printf ("This program is linked against GStreamer %d.%d.%d\n",
          major, minor, micro);

  rtvc::pipeline::source src_pipeline (host1, port1, user, password, 13, 0);
  rtvc::pipeline::visualization view_pipeline;
  bool reset_caps = false;

  src_pipeline.sample_signal.connect
    (
     [&] (GstSample* sample)
     {
       std::cout << "appsink" << std::endl;
       static GstClockTime timestamp_offset;

       GstBuffer* buffer = gst_sample_get_buffer (sample);
       if(!reset_caps)
       {
         timestamp_offset = GST_BUFFER_TIMESTAMP (buffer);
         GstCaps* caps = gst_sample_get_caps (sample);

         std::cout << "appsrc caps will be " << gst_caps_to_string (caps) << std::endl;
         
         gst_app_src_set_caps (GST_APP_SRC (view_pipeline.appsrc), caps);
       //   gst_app_src_set_caps (GST_APP_SRC (motion_pipeline.appsrc), caps);
         gst_caps_unref (caps);
         reset_caps = true;

         GstBuffer* tmp = gst_buffer_copy (buffer);
         GstBuffer* tmp1 = gst_buffer_copy (buffer);
         GST_BUFFER_TIMESTAMP (tmp) = 0;
         GST_BUFFER_TIMESTAMP (tmp1) = 0;
         GstFlowReturn r;
         if ((r = gst_app_src_push_buffer (GST_APP_SRC(view_pipeline.appsrc), tmp)) != GST_FLOW_OK)
         {
           std::cout << "Error with gst_app_src_push_buffer for view_pipeline, return " << r << std::endl;
         }

         gst_pipeline_set_latency(GST_PIPELINE(view_pipeline.pipeline), GST_SECOND);
         gst_element_set_state(view_pipeline.pipeline, GST_STATE_PLAYING);
       }
       else
       {
         // std::cout << "sending buffer" << std::endl;
         GstBuffer* tmp = gst_buffer_copy (buffer);
         GstBuffer* tmp1 = gst_buffer_copy (buffer);
         GST_BUFFER_TIMESTAMP (tmp) -= timestamp_offset;
         GST_BUFFER_TIMESTAMP (tmp1) -= timestamp_offset;
         GstFlowReturn r;
         if ((r = gst_app_src_push_buffer (GST_APP_SRC(view_pipeline.appsrc), tmp)) != GST_FLOW_OK)
         {
           std::cout << "Error with gst_app_src_push_buffer for view_pipeline, return " << r << std::endl;
         }
       }
     }
    );
  
  gst_element_set_state(src_pipeline.pipeline, GST_STATE_READY);
  gst_element_set_state(view_pipeline.pipeline, GST_STATE_READY);

  GMainLoop* main_loop = g_main_loop_new (NULL, FALSE);

  gst_element_set_state (src_pipeline.pipeline, GST_STATE_PLAYING);
  
  GstBus* bus = gst_element_get_bus (src_pipeline.pipeline);
  gst_bus_add_signal_watch (bus);


  GError *err;
  gchar *debug_info;

  /* Print error details on the screen */
  auto error_callback = [&] (GstBus *bus, GstMessage *msg)
     {
       gst_message_parse_error (msg, &err, &debug_info);
       g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
       g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
       
       if (!strcmp(GST_OBJECT_NAME(msg->src), "dmsssrc"))
       {
         std::cout << "Error happened in dmsssrc" << std::endl;

         gst_element_set_state(view_pipeline.pipeline, GST_STATE_PAUSED);
         reset_caps = false;
         gst_element_set_state(src_pipeline.pipeline, GST_STATE_READY);
         gst_element_set_state(src_pipeline.pipeline, GST_STATE_PLAYING);
         gst_element_set_state(view_pipeline.pipeline, GST_STATE_PLAYING);
       }
  
       g_clear_error (&err);
       g_free (debug_info);
  
       // g_main_loop_quit (data->main_loop);
     };

  g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)error_cb<decltype(error_callback)>, &error_callback);
  gst_object_unref (GST_OBJECT (bus));
  
  g_main_loop_run (main_loop);
 
  return 0;
}
