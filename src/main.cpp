
#include <rtvc/pipeline/source.hpp>
#include <rtvc/pipeline/visualization.hpp>

#include <gst/gst.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappsrc.h>

#include <stdio.h>
#include <iostream>

#include <boost/program_options.hpp>
#include <boost/dynamic_bitset.hpp>


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

  std::vector<std::string> hosts;
  std::string host2, user, password;
  std::vector<int> ports;
  std::vector<int> channels;
  int port2;
  
  {
    namespace po = boost::program_options;
    po::options_description desc("Allowed options");
    desc.add_options()
      ("help", "produce help message")
      ("host", po::value<std::vector<std::string>>()->multitoken(), "Connection to NVR")
      ("port", po::value<std::vector<int>>()->multitoken(), "Port for connection to NVR")
      ("failover-host", po::value<std::string>(), "Connection failover to NVR")
      ("failover-port", po::value<int>(), "Port for connection failover to NVR")
      ("user", po::value<std::string>(), "User for connection to NVR")
      ("pass", po::value<std::string>(), "Password for connection to NVR")
      ("channel", po::value<std::vector<int>>(), "Channel to show")
      ("width", po::value<unsigned int>(), "Width of the Window")
      ("height", po::value<unsigned int>(), "Height of the Window")
      ;

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);    

    if (vm.count("help")
        || !vm.count("host")
        || !vm.count("port")
        || !vm.count("user")
        || !vm.count("pass")
        || !vm.count("channel"))
    {
      std::cout << desc << "\n";
      return 1;
    }

    hosts = vm["host"].as<std::vector<std::string>>();
    user = vm["user"].as<std::string>();
    password = vm["pass"].as<std::string>();
    ports = vm["port"].as<std::vector<int>>();
    channels = vm["channel"].as<std::vector<int>>();

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

  std::vector<rtvc::pipeline::source> sources(hosts.size());
  rtvc::pipeline::visualization view_pipeline(hosts.size());
  boost::dynamic_bitset<> sources_loaded(hosts.size());
  boost::dynamic_bitset<> reset_caps(hosts.size());
  {
    unsigned int index = 0;
    for (auto&& host : hosts)
    {
      std::cout << "initializing source" << std::endl;
      sources[index] = std::move(rtvc::pipeline::source{host, ports[index], user, password, channels[index], 1});
      std::cout << "moved afaik" << std::endl;
      sources[index].sample_signal.connect
        (
         [&,index] (GstSample* sample)
         {
           std::cout << "appsink " << index << std::endl;
           static GstClockTime timestamp_offset;

           GstBuffer* buffer = gst_sample_get_buffer (sample);
           if(!reset_caps[index])
           {
             timestamp_offset = GST_BUFFER_TIMESTAMP (buffer);
             GstCaps* caps = gst_sample_get_caps (sample);

             std::cout << "appsrc caps will be " << gst_caps_to_string (caps) << std::endl;
         
             gst_app_src_set_caps (GST_APP_SRC (view_pipeline.appsrc[index]), caps);
             //   gst_app_src_set_caps (GST_APP_SRC (motion_pipeline.appsrc), caps);
             gst_caps_unref (caps);
             reset_caps[index] = true;

             GstBuffer* tmp = gst_buffer_copy (buffer);
             GstBuffer* tmp1 = gst_buffer_copy (buffer);
             GST_BUFFER_TIMESTAMP (tmp) = 0;
             GST_BUFFER_TIMESTAMP (tmp1) = 0;
             GstFlowReturn r;
             if ((r = gst_app_src_push_buffer (GST_APP_SRC(view_pipeline.appsrc[index]), tmp)) != GST_FLOW_OK)
             {
               std::cout << "Error with gst_app_src_push_buffer for view_pipeline, return " << r << std::endl;
             }
             sources_loaded[index] = true;

             if (sources_loaded.none())
             {
               std::cout << "starting view pipeline" << std::endl;
               gst_element_set_state(view_pipeline.pipeline, GST_STATE_PLAYING);
             }
             else
             {
               std::cout << "starting view pipeline" << std::endl;
               gst_element_set_state(view_pipeline.pipeline, GST_STATE_READY);
               gst_element_set_state(view_pipeline.pipeline, GST_STATE_PLAYING);
             }
           }
           else
           {
             // std::cout << "sending buffer" << std::endl;
             GstBuffer* tmp = gst_buffer_copy (buffer);
             GstBuffer* tmp1 = gst_buffer_copy (buffer);
             GST_BUFFER_TIMESTAMP (tmp) -= timestamp_offset;
             GST_BUFFER_TIMESTAMP (tmp1) -= timestamp_offset;
             GstFlowReturn r;
             if ((r = gst_app_src_push_buffer (GST_APP_SRC(view_pipeline.appsrc[index]), tmp)) != GST_FLOW_OK)
             {
               std::cout << "Error with gst_app_src_push_buffer for view_pipeline, return " << r << std::endl;
             }
           }
         }
         );
      ++index;
    }

  }

  gst_pipeline_set_latency(GST_PIPELINE(view_pipeline.pipeline), GST_SECOND);

  for (auto&& source : sources)
  {
    gst_element_set_state(source.pipeline, GST_STATE_READY);
  }
  gst_element_set_state(view_pipeline.pipeline, GST_STATE_READY);

  GMainLoop* main_loop = g_main_loop_new (NULL, FALSE);

  unsigned int index = 0;
  for (auto&& source : sources)
  {
    gst_element_set_state (source.pipeline, GST_STATE_PLAYING);
    GstBus* bus = gst_element_get_bus (source.pipeline);
    gst_bus_add_signal_watch (bus);

    GError *err;
    gchar *debug_info;

    /* Print error details on the screen */
    auto error_callback = [&,index] (GstBus *bus, GstMessage *msg)
     {
       gst_message_parse_error (msg, &err, &debug_info);
       g_printerr ("Error received from element %s: %s\n", GST_OBJECT_NAME (msg->src), err->message);
       g_printerr ("Debugging information: %s\n", debug_info ? debug_info : "none");
       
       if (!strcmp(GST_OBJECT_NAME(msg->src), "dmsssrc"))
       {
         std::cout << "Error happened in dmsssrc" << std::endl;

         //gst_element_set_state(view_pipeline.pipeline, GST_STATE_PAUSED);
         reset_caps[index] = false;
         gst_element_set_state(sources[index].pipeline, GST_STATE_READY);
         gst_element_set_state(sources[index].pipeline, GST_STATE_PLAYING);
         //gst_element_set_state(view_pipeline.pipeline, GST_STATE_PLAYING);
       }
  
       g_clear_error (&err);
       g_free (debug_info);
  
       // g_main_loop_quit (data->main_loop);
     };

    typedef decltype(error_callback) error_callback_type;
    g_signal_connect (G_OBJECT (bus), "message::error", (GCallback)error_cb<error_callback_type>, new error_callback_type(error_callback));
    gst_object_unref (GST_OBJECT (bus));
    ++index;
  }

  
  g_main_loop_run (main_loop);
 
  return 0;
}
