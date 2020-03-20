///////////////////////////////////////////////////////////////////////////////
//
// Copyright 2018 Felipe Magno de Almeida.
// Distributed under the Boost Software License, Version 1.0. (See
// accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
// See http://www.boost.org/libs/foreach for documentation
//

#ifndef RTVC_PIPELINE_VISUALIZATION_HPP
#define RTVC_PIPELINE_VISUALIZATION_HPP

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <string>
#include <stdexcept>

namespace rtvc { namespace pipeline {

struct visualization
{
  std::vector<GstElement*> appsrc;
  std::vector<GstElement*> videoconvert;
  GstElement *videomixer;
  GstElement *sink;
  GstElement *pipeline;

  visualization (int sources)
    : videomixer (gst_element_factory_make ("videomixer", "videomixer"))
    , sink (gst_element_factory_make ("fpsdisplaysink", "autovideosink"))
    , pipeline (gst_pipeline_new ("pipeline"))
  {
    appsrc.resize(sources);
    videoconvert.resize(sources);
    if (!videomixer || !sink || !pipeline)
    {
      throw std::runtime_error ("Not all elements could be created in visualization.");
    }
    gst_bin_add_many (GST_BIN (pipeline), videomixer, sink, NULL);

    {
      unsigned int i = 0;
      for (auto&& src : appsrc)
      {
        std::string name = "appsrc";
        name += std::to_string(++i);
        src = gst_element_factory_make ("appsrc", name.c_str());
        if (!src)
          throw std::runtime_error ("Not all elements could be created in visualization.");
        g_object_set (G_OBJECT (src), "format", GST_FORMAT_TIME, NULL);
        g_object_set (G_OBJECT (src), "is-live", TRUE, NULL);
        gst_app_src_set_stream_type(GST_APP_SRC(src), GST_APP_STREAM_TYPE_STREAM);
        gst_bin_add (GST_BIN (pipeline), src);
      }

      i = 0;
      for (auto&& convert : videoconvert)
      {
        std::cout << "creating videoconvert elements" << std::endl;
        std::string name = "videoconvert";
        name += std::to_string(i);
        convert = gst_element_factory_make ("videoconvert", name.c_str());
        if (!convert)
          throw std::runtime_error ("Not all elements could be created in visualization.");
        std::cout << "created" << std::endl;
        gst_bin_add (GST_BIN (pipeline), convert);
        gst_element_link (appsrc[i], convert);
        auto src_pad = gst_element_get_static_pad (convert, "src");
        assert (!!src_pad);
        auto sink_pad = gst_element_get_request_pad (videomixer, "sink_%u");
        assert (!!sink_pad);

        if (i == 0)
          g_object_set (G_OBJECT (sink_pad), "xpos", 0, "ypos", 0, NULL);
        else if (i == 1)
          g_object_set (G_OBJECT (sink_pad), "xpos", 1920, "ypos", 0, NULL);
        else if (i == 2)
          g_object_set (G_OBJECT (sink_pad), "xpos", 0, "ypos", 1000, NULL);
        else 
          g_object_set (G_OBJECT (sink_pad), "xpos", 1920, "ypos", 1000, NULL);

        std::cout << "pad name: " << gst_pad_get_name (sink_pad) << std::endl;
        gst_pad_link (src_pad, sink_pad);

        ++i;
      }
    }
    
    if (gst_element_link_many (videomixer, sink, NULL) != TRUE)
    {
      throw std::runtime_error ("Elements could not be linked.\n");
    }    
    
  }
};
    
} }

#endif
