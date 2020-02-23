///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2018 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef RTVC_SRC_PIPELINE_HPP
#define RTVC_SRC_PIPELINE_HPP

#include <gst/gst.h>
#include <gst/app/gstappsink.h>

#include <string>
#include <stdexcept>
#include <iostream>

#include <boost/signals2.hpp>

namespace rtvc { namespace pipeline {

struct source
{
  GstElement *dmsssrc;
  GstElement *dmssdemux;
  // GstElement *decodebin_video;
  GstElement *h264parse;
  GstElement *h264dec;
  GstElement *videoconvert;
  GstElement *motion_appsrc;
  GstElement *appsink;
  GstElement *pipeline;

  source (std::string const& host, unsigned short port, std::string username
          , std::string const& password
          , unsigned int channel, unsigned int subchannel)
    : dmsssrc (gst_element_factory_make ("dmsssrc", "dmsssrc"))
    , dmssdemux (gst_element_factory_make ("dmssdemux", "dmssdemux"))
    // , decodebin_video (gst_element_factory_make ("decodebin", "decodebin_video"))
    , h264parse (gst_element_factory_make ("h264parse", "h264parse"))
    , h264dec (gst_element_factory_make ("vaapih264dec", "h264dec"))
    , videoconvert (gst_element_factory_make ("videoconvert", "videoconvert"))
    , appsink (gst_element_factory_make ("appsink", "video_appsink"))
    , pipeline (gst_pipeline_new ("pipeline"))
  {
    if (!dmsssrc || !dmssdemux || /*!decodebin_video ||*/ !appsink || !pipeline)
    {
      throw std::runtime_error ("Not all elements could be created.");
    }

    GstCaps* memory_caps = gst_caps_new_simple("video/x-raw", "format", G_TYPE_STRING, "RGB", NULL);
    // GstCaps* memory_caps = gst_caps_new_empty_simple("video/x-raw");
    gst_app_sink_set_caps ( GST_APP_SINK(appsink), memory_caps);
    gst_caps_unref (memory_caps);

    g_object_set (G_OBJECT (dmsssrc), "host", host.c_str(), "port", port, "user", username.c_str(), "password", password.c_str()
                  , "channel", channel, "subchannel", subchannel, NULL);
    g_object_set (G_OBJECT (dmsssrc), "timeout", 15, NULL);

    // GstPad* appsink_sinkpad = gst_element_get_static_pad (videoconvert, "sink");
    // g_signal_connect (decodebin_video, "pad-added", G_CALLBACK (decodebin_newpad), appsink_sinkpad);
    
    GstAppSinkCallbacks callbacks
      = {
         &appsink_eos
         , &appsink_preroll
         , &appsink_sample
        };

    gst_app_sink_set_callbacks ( GST_APP_SINK(appsink), &callbacks, this, NULL);

    gst_bin_add_many (GST_BIN (pipeline), dmsssrc, dmssdemux, h264parse, h264dec, appsink, videoconvert, NULL);
    if (gst_element_link_many (dmsssrc, dmssdemux, h264parse, h264dec, videoconvert, appsink, NULL) != TRUE
        // || gst_element_link_filtered (videoconvert, appsink, NULL) != TRUE
        )
    {
      gst_object_unref (pipeline);
      throw std::runtime_error ("Elements could not be linked");
    }        
  }

  boost::signals2::signal <void (GstSample*)> sample_signal;  
private:
  static void decodebin_newpad (GstElement *decodebin, GstPad *pad, gpointer data)
  {
    GstPad* sinkpad = static_cast<GstPad*>(data);

    if (!GST_PAD_IS_LINKED (sinkpad))
    {
      gst_pad_link (pad, sinkpad);
    }
    else
    {
      // 
    }
  }

  static void appsink_eos (GstAppSink *appsink, gpointer user_data)
  {
    
  }
  static GstFlowReturn appsink_preroll (GstAppSink *appsink, gpointer user_data)
  {
    std::cout << "preroll" << std::endl;
    return GST_FLOW_OK;
  }
  static GstFlowReturn appsink_sample (GstAppSink *appsink, gpointer user_data)
  {
    source* self = static_cast<source*>(user_data);
    
    GstSample* sample = gst_app_sink_pull_sample (GST_APP_SINK (appsink));
    self->sample_signal (sample);
    gst_sample_unref (sample);
    
    return GST_FLOW_OK;
  }
};
  
} }

#endif
