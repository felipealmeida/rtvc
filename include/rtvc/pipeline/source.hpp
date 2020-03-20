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
  GstElement *h264parse;
  GstElement *h264dec;
  GstElement *videoconvert;
  GstElement *appsink;
  GstElement *pipeline;

  source() : dmsssrc (nullptr), dmssdemux(nullptr), h264parse(nullptr)
           , h264dec(nullptr), videoconvert(nullptr)
           , appsink(nullptr), pipeline(nullptr)
           , sample_signal{}
  {
    std::cout << "default constructor " << this << std::endl;
  }
  source (std::string const& host, unsigned short port, std::string username
          , std::string const& password
          , unsigned int channel, unsigned int subchannel)
    : dmsssrc (gst_element_factory_make ("dmsssrc", "dmsssrc"))
    , dmssdemux (gst_element_factory_make ("dmssdemux", "dmssdemux"))
    , h264parse (gst_element_factory_make ("h264parse", "h264parse"))
    , h264dec (gst_element_factory_make ("vaapih264dec", "h264dec"))
    , videoconvert (gst_element_factory_make ("videoconvert", "videoconvert"))
    , appsink (gst_element_factory_make ("appsink", "video_appsink"))
    , pipeline (gst_pipeline_new ("pipeline"))
  {
    std::cout << "normal constructor " << this << std::endl;
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

    std::cout << "this is " << this << std::endl;
    
    GstAppSinkCallbacks callbacks
      = {
         &appsink_eos
         , &appsink_preroll
         , &appsink_sample
        };

    gst_app_sink_set_callbacks ( GST_APP_SINK(appsink), &callbacks, this, appsink_notify_destroy);

    gst_bin_add_many (GST_BIN (pipeline), dmsssrc, dmssdemux, h264parse, h264dec, appsink, videoconvert, NULL);
    if (gst_element_link_many (dmsssrc, dmssdemux, h264parse, h264dec, videoconvert, appsink, NULL) != TRUE
        // || gst_element_link_filtered (videoconvert, appsink, NULL) != TRUE
        )
    {
      gst_object_unref (pipeline);
      throw std::runtime_error ("Elements could not be linked");
    }        
  }

  ~source ()
  {
    if (dmsssrc)
    {
      std::cout << "should free elements" << std::endl;
    }
    else
    {
      std::cout << "already moved object" << std::endl;
    }
  }
  
  source (source const&) = delete;

  void swap (source& other)
  {
    using std::swap;
    std::swap(dmsssrc, other.dmsssrc);
    std::swap(dmssdemux, other.dmssdemux);
    std::swap(h264parse, other.h264parse);
    std::swap(h264dec, other.h264dec);
    std::swap(videoconvert, other.videoconvert);
    std::swap(appsink, other.appsink);
    std::swap(pipeline, other.pipeline);
    swap(sample_signal, other.sample_signal);
    if (appsink)
    {
      GstAppSinkCallbacks callbacks
        = {
           &appsink_eos
           , &appsink_preroll
           , &appsink_sample
          };

      gst_app_sink_set_callbacks ( GST_APP_SINK(appsink), &callbacks, this, appsink_notify_destroy);
    }
    if (other.appsink)
    {
      GstAppSinkCallbacks callbacks
        = {
           &appsink_eos
           , &appsink_preroll
           , &appsink_sample
          };

      gst_app_sink_set_callbacks ( GST_APP_SINK(other.appsink), &callbacks, &other, appsink_notify_destroy);
    }

  }
  
  source& operator=(source other)
  {
    std::cout << "move operator assignment " << this << " from " << &other << std::endl;
    other.swap(*this);
    return *this;
  }
  
  source (source && other)
    : dmsssrc(other.dmsssrc), dmssdemux(other.dmssdemux), h264parse(other.h264parse)
    , h264dec(other.h264dec), videoconvert(other.videoconvert)
    , appsink(other.appsink), pipeline(other.pipeline), sample_signal(std::move(other.sample_signal))
  {
    std::cout << "move constructor " << this << " from " << &other << std::endl;
    other.dmsssrc = nullptr;
    other.dmssdemux = nullptr;
    other.h264parse = nullptr;
    other.h264dec = nullptr;
    other.videoconvert = nullptr;
    other.appsink = nullptr;
    std::cout << "MOVED this is " << this << std::endl;
    GstAppSinkCallbacks callbacks
      = {
         &appsink_eos
         , &appsink_preroll
         , &appsink_sample
        };

    gst_app_sink_set_callbacks ( GST_APP_SINK(appsink), &callbacks, this, appsink_notify_destroy);
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
  static void appsink_notify_destroy (gpointer user_data)
  {
  }
  static GstFlowReturn appsink_sample (GstAppSink *appsink, gpointer user_data)
  {
    std::cout << "appsink sample " << user_data << std::endl;
    source* self = static_cast<source*>(user_data);

    assert (!!self->appsink);
    assert (self->appsink == GST_ELEMENT(appsink));
    
    GstSample* sample = gst_app_sink_pull_sample (GST_APP_SINK (appsink));
    self->sample_signal (sample);
    gst_sample_unref (sample);

    return GST_FLOW_OK;
  }
};
  
} }

#endif
