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
  GstElement *appsrc;
  GstElement *videoconvert;
  GstElement *sink;
  GstElement *pipeline;

  visualization ()
    : appsrc (gst_element_factory_make ("appsrc", "appsrc"))
    , videoconvert (gst_element_factory_make ("videoconvert", "videoconvert"))
    , sink (gst_element_factory_make ("fpsdisplaysink", "autovideosink"))
    , pipeline (gst_pipeline_new ("pipeline"))
  {
    if (!appsrc || !videoconvert || !sink || !pipeline)
    {
      throw std::runtime_error ("Not all elements could be created.");
    }

    g_object_set (G_OBJECT (appsrc), "format", GST_FORMAT_TIME, NULL);
    g_object_set (G_OBJECT (appsrc), "is-live", TRUE, NULL);
    gst_app_src_set_stream_type(GST_APP_SRC(appsrc), GST_APP_STREAM_TYPE_STREAM);

    gst_bin_add_many (GST_BIN (pipeline), appsrc, videoconvert, sink, NULL);
    if (gst_element_link_many (appsrc, videoconvert, sink, NULL) != TRUE)
    {
      throw std::runtime_error ("Elements could not be linked.\n");
    }    
    
  }
};
    
} }

#endif
