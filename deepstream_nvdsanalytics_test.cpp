/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: LicenseRef-NvidiaProprietary
 *
 * NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
 * property and proprietary rights in and to this material, related
 * documentation and any modifications thereto. Any use, reproduction,
 * disclosure or distribution of this material and related documentation
 * without an express license agreement from NVIDIA CORPORATION or
 * its affiliates is strictly prohibited.
 */

 #include <gst/gst.h>
 #include <glib.h>
 #include <stdio.h>
 #include <math.h>
 #include <string.h>
 #include <sys/time.h>
 #include <iostream>
 #include <vector>
 #include <unordered_map>
 #include <sstream>
 #include <cuda_runtime_api.h>
 #include "gstnvdsmeta.h"
 #include "nvds_analytics_meta.h"
 #ifndef PLATFORM_TEGRA
 #include "gst-nvmessage.h"
 #endif
 #include <sys/socket.h>
 #include <netinet/in.h>
 #include <arpa/inet.h>
 #include <unistd.h>
 #include <fcntl.h> // Thêm header cho fcntl
 #include "sendTCP.h" // Đảm bảo include sendTCP.h để dùng SendTCP_Traffic

 
 #define MAX_DISPLAY_LEN 64
 
 #define PGIE_CLASS_ID_VEHICLE 0
 #define PGIE_CLASS_ID_PERSON 2
 
 #define MUXER_OUTPUT_WIDTH 1920
 #define MUXER_OUTPUT_HEIGHT 1080
 
 #define MUXER_BATCH_TIMEOUT_USEC 4000000
 
 #define TILED_OUTPUT_WIDTH 1920
 #define TILED_OUTPUT_HEIGHT 1080
 
 #define NVINFER_PLUGIN "nvinfer"
 #define NVINFERSERVER_PLUGIN "nvinferserver"
 
 #define PGIE_CONFIG_FILE  "nvdsanalytics_pgie_config.txt"
 #define PGIE_NVINFERSERVER_CONFIG_FILE "nvdsanalytics_pgie_nvinferserver_config.txt"
 
 #define GST_CAPS_FEATURES_NVMM "memory:NVMM"
 
 gchar pgie_classes_str[4][32] = { "Vehicle", "TwoWheeler", "Person", "RoadSign" };
 
 // Khai báo socket toàn cục
 static int sock = 0;
 static struct sockaddr_in serv_addr;
 
 static void initialize_socket() {
     printf("hungdan\n");
     initialize_socketcc();
 
    //  sock = socket(AF_INET, SOCK_STREAM, 0);
    //  if (sock < 0) {
    //      printf("Socket creation error\n");
    //      return;
    //  }
 
    //  // Thiết lập socket thành non-blocking
    //  int flags = fcntl(sock, F_GETFL, 0);
    //  if (flags == -1) {
    //      printf("Failed to get socket flags: %s\n", strerror(errno));
    //      close(sock);
    //      sock = 0;
    //      return;
    //  }
    //  if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
    //      printf("Failed to set socket to non-blocking: %s\n", strerror(errno));
    //      close(sock);
    //      sock = 0;
    //      return;
    //  }
 
    //  serv_addr.sin_family = AF_INET;
    //  serv_addr.sin_port = htons(5000);
 
    //  if (inet_pton(AF_INET, "192.168.0.10", &serv_addr.sin_addr) <= 0) {
    //      printf("Invalid address\n");
    //      close(sock);
    //      sock = 0;
    //      return;
    //  }
 
    //  // Kết nối non-blocking
    //  int ret = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    //  if (ret < 0) {
    //      if (errno == EINPROGRESS) {
    //          printf("Connection in progress (non-blocking)\n");
    //          // Kết nối đang được thực hiện, cần kiểm tra sau
    //          fd_set writefds;
    //          struct timeval tv;
    //          FD_ZERO(&writefds);
    //          FD_SET(sock, &writefds);
    //          tv.tv_sec = 5; // Timeout 5 giây
    //          tv.tv_usec = 0;
 
    //          ret = select(sock + 1, NULL, &writefds, NULL, &tv);
    //          if (ret > 0 && FD_ISSET(sock, &writefds)) {
    //              int so_error;
    //              socklen_t len = sizeof(so_error);
    //              getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
    //              if (so_error == 0) {
    //                  printf("Socket connected successfully\n");
    //              } else {
    //                  printf("Socket connection failed: %s\n", strerror(so_error));
    //                  close(sock);
    //                  sock = 0;
    //              }
    //          } else {
    //              printf("Connection timed out or failed\n");
    //              close(sock);
    //              sock = 0;
    //          }
    //      } else {
    //          printf("Connection failed immediately: %s\n", strerror(errno));
    //          close(sock);
    //          sock = 0;
    //      }
    //  } else {
    //      printf("Socket connected immediately\n");
    //  }
 }
 
 #include "sendTCP.h" // Đảm bảo include sendTCP.h để dùng SendTCP_Traffic

static GstPadProbeReturn
nvdsanalytics_src_pad_buffer_probe(GstPad *pad, GstPadProbeInfo *info, gpointer u_data)
{
    GstBuffer *buf = (GstBuffer *)info->data;
    NvDsBatchMeta *batch_meta = gst_buffer_get_nvds_batch_meta(buf);
    if (!batch_meta) {
        g_printerr("No batch meta available\n");
        return GST_PAD_PROBE_OK;
    }

    for (NvDsMetaList *l_frame = batch_meta->frame_meta_list; l_frame != NULL; l_frame = l_frame->next) {
        NvDsFrameMeta *frame_meta = (NvDsFrameMeta *)(l_frame->data);
        if (!frame_meta) continue;

        std::stringstream out_string;
        guint num_rects = 0, vehicle_count = 0, person_count = 0;
        uint32_t obj_in_roi_count = 0; // Biến mới để lưu số lượng đối tượng trong ROI

        // Tính toán số lượng đối tượng (giữ nguyên logic cũ để in thông tin)
        for (NvDsMetaList *l_obj = frame_meta->obj_meta_list; l_obj != NULL; l_obj = l_obj->next) {
            NvDsObjectMeta *obj_meta = (NvDsObjectMeta *)(l_obj->data);
            if (!obj_meta) continue;

            if (obj_meta->class_id == PGIE_CLASS_ID_VEHICLE) {
                vehicle_count++;
                num_rects++;
            }
            if (obj_meta->class_id == PGIE_CLASS_ID_PERSON) {
                person_count++;
                num_rects++;
            }

            for (NvDsMetaList *l_user_meta = obj_meta->obj_user_meta_list; l_user_meta != NULL; l_user_meta = l_user_meta->next) {
                NvDsUserMeta *user_meta = (NvDsUserMeta *)l_user_meta->data;
                if (user_meta && user_meta->base_meta.meta_type == NVDS_USER_OBJ_META_NVDSANALYTICS) {
                    NvDsAnalyticsObjInfo *user_meta_data = (NvDsAnalyticsObjInfo *)user_meta->user_meta_data;
                    if (user_meta_data && user_meta_data->dirStatus.length()) {
                        g_print("object %lu moving in %s\n", obj_meta->object_id, user_meta_data->dirStatus.c_str());
                    }
                }
            }
        }

        // Xử lý metadata từ nvdsanalytics để lấy objInROIcnt
        for (NvDsMetaList *l_user = frame_meta->frame_user_meta_list; l_user != NULL; l_user = l_user->next) {
            NvDsUserMeta *user_meta = (NvDsUserMeta *)l_user->data;
            if (user_meta && user_meta->base_meta.meta_type == NVDS_USER_FRAME_META_NVDSANALYTICS) {
                NvDsAnalyticsFrameMeta *meta = (NvDsAnalyticsFrameMeta *)user_meta->user_meta_data;
                if (meta) {
                    // Lấy số lượng đối tượng trong ROI
                    for (const std::pair<std::string, uint32_t> &status : meta->objInROIcnt) {
                        obj_in_roi_count += status.second; // Tổng hợp số lượng đối tượng từ tất cả ROI
                        out_string << "Objs in ROI " << status.first << " = " << status.second << " ";
                    }
                    for (std::pair<std::string, uint32_t> status : meta->objLCCumCnt) {
                        out_string << "LineCrossing Cumulative " << status.first << " = " << status.second << " ";
                    }
                    for (std::pair<std::string, uint32_t> status : meta->objLCCurrCnt) {
                        out_string << "LineCrossing Current Frame " << status.first << " = " << status.second << " ";
                    }
                    for (std::pair<std::string, bool> status : meta->ocStatus) {
                        out_string << "Overcrowding status " << status.first << " = " << status.second << " ";
                    }
                }
            }
        }

        // // In thông tin ra console (giữ nguyên)
        g_print("Frame Number = %d of Stream = %d, Number of objects = %d "
                "Vehicle Count = %d Person Count = %d %s %d\n",
                frame_meta->frame_num, frame_meta->pad_index,
                num_rects, vehicle_count, person_count, out_string.str().c_str(),obj_in_roi_count);

        // Gửi dữ liệu qua TCP với laneID và obj_in_roi_count
        uint32_t laneID = frame_meta->pad_index; // "of Stream" -> laneID
        SendTCP_Traffic(laneID, obj_in_roi_count); // Gửi obj_in_roi_count thay vì vehicle_count
    }

    return GST_PAD_PROBE_OK;
}
 
 static gboolean
 bus_call (GstBus * bus, GstMessage * msg, gpointer data)
 {
   GMainLoop *loop = (GMainLoop *) data;
   switch (GST_MESSAGE_TYPE (msg)) {
     case GST_MESSAGE_EOS:
       g_print ("End of stream\n");
       g_main_loop_quit (loop);
       break;
     case GST_MESSAGE_WARNING:
     {
       gchar *debug;
       GError *error;
       gst_message_parse_warning (msg, &error, &debug);
       g_printerr ("WARNING from element %s: %s\n",
           GST_OBJECT_NAME (msg->src), error->message);
       g_free (debug);
       g_printerr ("Warning: %s\n", error->message);
       g_error_free (error);
       break;
     }
     case GST_MESSAGE_ERROR:
     {
       gchar *debug;
       GError *error;
       gst_message_parse_error (msg, &error, &debug);
       g_printerr ("ERROR from element %s: %s\n",
           GST_OBJECT_NAME (msg->src), error->message);
       if (debug)
         g_printerr ("Error details: %s\n", debug);
       g_free (debug);
       g_error_free (error);
       g_main_loop_quit (loop);
       break;
     }
 #ifndef PLATFORM_TEGRA
     case GST_MESSAGE_ELEMENT:
     {
       if (gst_nvmessage_is_stream_eos (msg)) {
         guint stream_id = 0;
         if (gst_nvmessage_parse_stream_eos (msg, &stream_id)) {
           g_print ("Got EOS from stream %d\n", stream_id);
         }
       }
       break;
     }
 #endif
     default:
       break;
   }
   return TRUE;
 }
 
 static void
 cb_newpad (GstElement * decodebin, GstPad * decoder_src_pad, gpointer data)
 {
   g_print ("In cb_newpad\n");
   GstCaps *caps = gst_pad_get_current_caps (decoder_src_pad);
   const GstStructure *str = gst_caps_get_structure (caps, 0);
   const gchar *name = gst_structure_get_name (str);
   GstElement *source_bin = (GstElement *) data;
   GstCapsFeatures *features = gst_caps_get_features (caps, 0);
 
   /* Need to check if the pad created by the decodebin is for video and not
    * audio. */
   if (!strncmp (name, "video", 5)) {
     /* Link the decodebin pad only if decodebin has picked nvidia
      * decoder plugin nvdec_*. We do this by checking if the pad caps contain
      * NVMM memory features. */
     if (gst_caps_features_contains (features, GST_CAPS_FEATURES_NVMM)) {
       /* Get the source bin ghost pad */
       GstPad *bin_ghost_pad = gst_element_get_static_pad (source_bin, "src");
       if (!gst_ghost_pad_set_target (GST_GHOST_PAD (bin_ghost_pad),
               decoder_src_pad)) {
         g_printerr ("Failed to link decoder src pad to source bin ghost pad\n");
       }
       gst_object_unref (bin_ghost_pad);
     } else {
       g_printerr ("Error: Decodebin did not pick nvidia decoder plugin.\n");
     }
   }
 }
 
 static void
 decodebin_child_added (GstChildProxy * child_proxy, GObject * object,
     gchar * name, gpointer user_data)
 {
   g_print ("Decodebin child added: %s\n", name);
   if (g_strrstr (name, "decodebin") == name) {
     g_signal_connect (G_OBJECT (object), "child-added",
         G_CALLBACK (decodebin_child_added), user_data);
   }
 }
 
 static GstElement *
 create_source_bin (guint index, gchar * uri)
 {
   GstElement *bin = NULL, *uri_decode_bin = NULL;
   gchar bin_name[16] = { };
 
   g_snprintf (bin_name, 15, "source-bin-%02d", index);
   /* Create a source GstBin to abstract this bin's content from the rest of the
    * pipeline */
   bin = gst_bin_new (bin_name);
 
   /* Source element for reading from the uri.
    * We will use decodebin and let it figure out the container format of the
    * stream and the codec and plug the appropriate demux and decode plugins. */
   uri_decode_bin = gst_element_factory_make ("uridecodebin", "uri-decode-bin");
 
   if (!bin || !uri_decode_bin) {
     g_printerr ("One element in source bin could not be created.\n");
     return NULL;
   }
 
   /* We set the input uri to the source element */
   g_object_set (G_OBJECT (uri_decode_bin), "uri", uri, NULL);
 
   /* Connect to the "pad-added" signal of the decodebin which generates a
    * callback once a new pad for raw data has beed created by the decodebin */
   g_signal_connect (G_OBJECT (uri_decode_bin), "pad-added",
       G_CALLBACK (cb_newpad), bin);
   g_signal_connect (G_OBJECT (uri_decode_bin), "child-added",
       G_CALLBACK (decodebin_child_added), bin);
 
   gst_bin_add (GST_BIN (bin), uri_decode_bin);
 
   /* We need to create a ghost pad for the source bin which will act as a proxy
    * for the video decoder src pad. The ghost pad will not have a target right
    * now. Once the decode bin creates the video decoder and generates the
    * cb_newpad callback, we will set the ghost pad target to the video decoder
    * src pad. */
   if (!gst_element_add_pad (bin, gst_ghost_pad_new_no_target ("src",
               GST_PAD_SRC))) {
     g_printerr ("Failed to add ghost pad in source bin\n");
     return NULL;
   }
 
   return bin;
 }
 
 static void
 usage(const char *bin)
 {
   g_printerr ("Usage: %s <uri1> [uri2] ... [uriN]\n", bin);
   g_printerr ("For nvinferserver, Usage: %s -t inferserver <uri1> [uri2] ... [uriN]\n", bin);
 }
 
 int main(int argc, char *argv[])
 {
     GMainLoop *loop = NULL;
     GstElement *pipeline = NULL, *streammux = NULL, *sink = NULL, *pgie = NULL,
                *nvtracker = NULL, *nvdsanalytics = NULL,
         *nvvidconv = NULL, *nvosd = NULL, *tiler = NULL,
         *queue1, *queue2, *queue3, *queue4, *queue5, *queue6, *queue7;
     GstBus *bus = NULL;
     guint bus_watch_id;
     GstPad *nvdsanalytics_src_pad = NULL;
     guint i, num_sources;
     guint tiler_rows, tiler_columns;
     guint pgie_batch_size;
     gboolean is_nvinfer_server = FALSE;
     const gchar* new_mux_str = g_getenv("USE_NEW_NVSTREAMMUX");
     gboolean use_new_mux = !g_strcmp0(new_mux_str, "yes");
 
     int current_device = -1;
     cudaGetDevice(&current_device);
     struct cudaDeviceProp prop;
     cudaGetDeviceProperties(&prop, current_device);
 
     if (argc < 2) {
         usage(argv[0]);
         return -1;
     }
 
     if (argc >= 2 && !strcmp("-t", argv[1])) {
         if (!strcmp("inferserver", argv[2])) {
             is_nvinfer_server = TRUE;
         } else {
             usage(argv[0]);
             return -1;
         }
         g_print("Using nvinferserver as the inference plugin\n");
     }
 
     if (is_nvinfer_server) {
         num_sources = argc - 3;
     } else {
         num_sources = argc - 1;
     }
 
     gst_init(&argc, &argv);
     loop = g_main_loop_new(NULL, FALSE);
 
     pipeline = gst_pipeline_new("nvdsanalytics-test-pipeline");
     streammux = gst_element_factory_make("nvstreammux", "stream-muxer");
 
     if (!pipeline || !streammux) {
         g_printerr("One element could not be created. Exiting.\n");
         return -1;
     }
     gst_bin_add(GST_BIN(pipeline), streammux);
 
     for (i = 0; i < num_sources; i++) {
         GstPad *sinkpad, *srcpad;
         GstElement *source_bin;
         gchar pad_name[16] = {};
         if (is_nvinfer_server) {
             source_bin = create_source_bin(i, argv[i + 3]);
         } else {
             source_bin = create_source_bin(i, argv[i + 1]);
         }
 
         if (!source_bin) {
             g_printerr("Failed to create source bin. Exiting.\n");
             return -1;
         }
 
         gst_bin_add(GST_BIN(pipeline), source_bin);
 
         g_snprintf(pad_name, 15, "sink_%u", i);
         sinkpad = gst_element_request_pad_simple(streammux, pad_name);
         if (!sinkpad) {
             g_printerr("Streammux request sink pad failed. Exiting.\n");
             return -1;
         }
 
         srcpad = gst_element_get_static_pad(source_bin, "src");
         if (!srcpad) {
             g_printerr("Failed to get src pad of source bin. Exiting.\n");
             return -1;
         }
 
         if (gst_pad_link(srcpad, sinkpad) != GST_PAD_LINK_OK) {
             g_printerr("Failed to link source bin to stream muxer. Exiting.\n");
             return -1;
         }
 
         gst_object_unref(srcpad);
         gst_object_unref(sinkpad);
     }
 
     pgie = gst_element_factory_make(
             is_nvinfer_server ? NVINFERSERVER_PLUGIN : NVINFER_PLUGIN,
             "primary-nvinference-engine");
 
     nvtracker = gst_element_factory_make("nvtracker", "nvtracker");
     nvdsanalytics = gst_element_factory_make("nvdsanalytics", "nvdsanalytics");
     tiler = gst_element_factory_make("nvmultistreamtiler", "nvtiler");
     nvvidconv = gst_element_factory_make("nvvideoconvert", "nvvideo-converter");
     nvosd = gst_element_factory_make("nvdsosd", "nv-onscreendisplay");
 
     queue1 = gst_element_factory_make("queue", "queue1");
     queue2 = gst_element_factory_make("queue", "queue2");
     queue3 = gst_element_factory_make("queue", "queue3");
     queue4 = gst_element_factory_make("queue", "queue4");
     queue5 = gst_element_factory_make("queue", "queue5");
     queue6 = gst_element_factory_make("queue", "queue6");
     queue7 = gst_element_factory_make("queue", "queue7");
 
     if (prop.integrated) {
         sink = gst_element_factory_make("nv3dsink", "nvvideo-renderer");
     } else {
 #ifdef __aarch64__
         sink = gst_element_factory_make("nv3dsink", "nvvideo-renderer");
 #else
         sink = gst_element_factory_make("nveglglessink", "nvvideo-renderer");
 #endif
     }
 
     if (!pgie || !nvtracker || !nvdsanalytics || !tiler || !nvvidconv ||
         !nvosd || !sink || !queue1 || !queue2 || !queue3 || !queue4 || !queue5 ||
         !queue6 || !queue7) {
         g_printerr("One element could not be created. Exiting.\n");
         return -1;
     }
 
     if (!use_new_mux) {
         g_object_set(G_OBJECT(streammux), "width", MUXER_OUTPUT_WIDTH, "height",
             MUXER_OUTPUT_HEIGHT, "batch-size", num_sources,
             "batched-push-timeout", MUXER_BATCH_TIMEOUT_USEC, NULL);
     } else {
         g_object_set(G_OBJECT(streammux), "batch-size", num_sources,
             "batched-push-timeout", MUXER_BATCH_TIMEOUT_USEC, NULL);
     }
 
     if (is_nvinfer_server) {
         g_object_set(G_OBJECT(pgie), "config-file-path", PGIE_NVINFERSERVER_CONFIG_FILE, NULL);
     } else {
         g_object_set(G_OBJECT(pgie), "config-file-path", PGIE_CONFIG_FILE, NULL);
     }
 
     g_object_set(G_OBJECT(nvtracker),
         "ll-lib-file", "/opt/nvidia/deepstream/deepstream/lib/libnvds_nvmultiobjecttracker.so",
         "ll-config-file", "../../../../samples/configs/deepstream-app/config_tracker_NvDCF_perf.yml",
         "tracker-width", 640, "tracker-height", 480,
         NULL);
 
     g_object_set(G_OBJECT(nvdsanalytics),
         "config-file", "config_nvdsanalytics.txt",
         NULL);
 
     g_object_get(G_OBJECT(pgie), "batch-size", &pgie_batch_size, NULL);
     if (pgie_batch_size != num_sources) {
         g_printerr("WARNING: Overriding infer-config batch-size (%d) with number of sources (%d)\n",
             pgie_batch_size, num_sources);
         g_object_set(G_OBJECT(pgie), "batch-size", num_sources, NULL);
     }
 
     tiler_rows = (guint) sqrt(num_sources);
     tiler_columns = (guint) ceil(1.0 * num_sources / tiler_rows);
     g_object_set(G_OBJECT(tiler), "rows", tiler_rows, "columns", tiler_columns,
         "width", TILED_OUTPUT_WIDTH, "height", TILED_OUTPUT_HEIGHT, NULL);
 
     g_object_set(G_OBJECT(sink), "qos", 0, NULL);
 
     bus = gst_pipeline_get_bus(GST_PIPELINE(pipeline));
     bus_watch_id = gst_bus_add_watch(bus, bus_call, loop);
     gst_object_unref(bus);
 
     gst_bin_add_many(GST_BIN(pipeline), queue1, pgie, queue2,
                 nvtracker, queue3, nvdsanalytics, queue4, tiler, queue5,
                 nvvidconv, queue6, nvosd, queue7, sink, NULL);
 
     if (!gst_element_link_many(streammux, queue1, pgie, queue2, nvtracker,
         queue3, nvdsanalytics, queue4, tiler, queue5, nvvidconv, queue6,
         nvosd, queue7, sink, NULL)) {
         g_printerr("Elements could not be linked. Exiting.\n");
         return -1;
     }
 
     nvdsanalytics_src_pad = gst_element_get_static_pad(nvdsanalytics, "src");
     if (!nvdsanalytics_src_pad)
         g_print("Unable to get src pad\n");
     else
         gst_pad_add_probe(nvdsanalytics_src_pad, GST_PAD_PROBE_TYPE_BUFFER,
             nvdsanalytics_src_pad_buffer_probe, NULL, NULL);
     gst_object_unref(nvdsanalytics_src_pad);
 
     g_print("Now playing...\n");
     gst_element_set_state(pipeline, GST_STATE_PLAYING);
 
     initialize_socket();
     g_print("Running...\n");
     g_main_loop_run(loop);
 
     g_print("Returned, stopping playback\n");
     gst_element_set_state(pipeline, GST_STATE_NULL);
     g_print("Deleting pipeline\n");
     gst_object_unref(GST_OBJECT(pipeline));
     g_source_remove(bus_watch_id);
     g_main_loop_unref(loop);
    //  if (sock > 0) {
    //      close(sock);
    //      sock = 0;
    //  }
     return 0;
 }
