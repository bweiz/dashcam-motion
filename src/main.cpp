#include <iostream>
#include <gst/gst.h>
#include <string.h>
#include <stdio.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <cstdint>
#include <fstream>
#include <vector>

struct MotionState { 
    GstSample* previous = nullptr; 
    bool active = false;
    std::vector<std::uint8_t> mask;
    std::ofstream mask_file{"changes.gray", std::ios::binary};
};

/* Helper Function */

static bool map_sample(GstSample* sample, GstVideoFrame* out) {
    GstCaps* caps = gst_sample_get_caps(sample);
    GstBuffer* buffer = gst_sample_get_buffer(sample);
    GstVideoInfo info{};

    if (!caps || !buffer || !gst_video_info_from_caps(&info, caps)) { return false; }

    return gst_video_frame_map(out, &info, buffer, GST_MAP_READ);
}

/* END Helper Function */

static GstFlowReturn on_frame(GstAppSink* sink, gpointer user_data) {

    auto* motion = static_cast<MotionState*>(user_data);

    GstSample* current = gst_app_sink_pull_sample(sink);
    if (!current) return GST_FLOW_EOS;

    GstVideoFrame current_frame{};
    if (!map_sample(current, &current_frame)) {
        gst_sample_unref(current);
        return GST_FLOW_ERROR;
    }

    const int current_width = GST_VIDEO_FRAME_WIDTH(&current_frame);
    const int current_height = GST_VIDEO_FRAME_HEIGHT(&current_frame);
    motion->mask.assign(static_cast<std::size_t>(current_width) * current_height, 0);

    std::size_t changed = 0;

    if (motion->previous) {
        int threshold = 20;

        GstVideoFrame previous_frame{};
        if (!map_sample(motion->previous, &previous_frame)) {
            gst_video_frame_unmap(&current_frame);
            gst_sample_unref(current);
            return GST_FLOW_ERROR;
        };

        const auto* current_pixels =  GST_VIDEO_FRAME_COMP_DATA(&current_frame, 0);
        const int current_stride = GST_VIDEO_FRAME_PLANE_STRIDE(&current_frame, 0);

        const auto* previous_pixels = GST_VIDEO_FRAME_COMP_DATA(&previous_frame, 0);
        const int previous_width = GST_VIDEO_FRAME_WIDTH(&previous_frame);
        const int previous_height = GST_VIDEO_FRAME_HEIGHT(&previous_frame);
        const int previous_stride = GST_VIDEO_FRAME_PLANE_STRIDE(&previous_frame, 0);

        for (int y = 0; y < current_height; ++y) {
            const auto* current_row = current_pixels + static_cast<gssize>(y) * current_stride;
            const auto* previous_row = previous_pixels + static_cast<gssize>(y) * previous_stride;
            auto* mask_row = motion->mask.data() + static_cast<std::size_t>(y) * current_width;

            for (int x = 0; x < current_width; x++) {
                const int difference = int(current_row[x]) - int(previous_row[x]);

                if (std::abs(difference) >= threshold) {
                    ++changed;
                    mask_row[x] = 255;
                }
            }
        }
        /*
        for(int i = 0; i < current_width * current_height; i++) {
            if (std::abs(int(current_pixels[i]) - int(previous_pixels[i])) >= threshold) {
                ++changed;

                motion->mask[static_cast<std::size_t>(y) * width + i] = 255;
            }
            const bool active_now = changed <= 700 || changed >= 900;
            if (active_now != motion->active) {
                std::cout << (active_now ? "motion started\n" : "motion ended\n");
                motion->active = active_now;
            }
        }
        */

        if (!motion->active && changed >= 900) {
            motion-> active = true;
            std::cout << "Motion Started\n";
        } else if (motion->active && changed <= 700) {
            motion->active = false;
            std::cout << "Motion Ended\n";
        } 

        gst_video_frame_unmap(&previous_frame);
        gst_sample_unref(motion->previous);
    }
    /* 
    GstVideoInfo info{};
    GstVideoFrame frame{};
    GstCaps* caps = gst_sample_get_caps(sample);
    GstBuffer* buffer = gst_sample_get_buffer(sample);

    if (    !caps || !buffer || !gst_video_info_from_caps(&info, caps) || 
            !gst_video_frame_map(&frame, &info, buffer, GST_MAP_READ)) {

        gst_sample_unref(sample);
        return GST_FLOW_ERROR;
    }
    
    const auto* pixels = GST_VIDEO_FRAME_COMP_DATA(&frame, 0);
    const int width = GST_VIDEO_FRAME_WIDTH(&frame);
    const int height = GST_VIDEO_FRAME_HEIGHT(&frame);
    const int stride = GST_VIDEO_FRAME_PLANE_STRIDE(&frame, 0);

    const auto* middle_row = pixels + (height / 2) * static_cast<gssize>(stride);
    std::cout << width << "x" << height << "  center pixel = " << static_cast<int>(middle_row[width / 2]) << '\n';

    */
    motion->mask_file.write(
            reinterpret_cast<const char*>(motion->mask.data()),
            static_cast<std::streamsize>(motion->mask.size()));

    std::cout << "Changed Pixels: " << changed << '\n';
    gst_video_frame_unmap(&current_frame);
    motion->previous = current;
    return GST_FLOW_OK;
}

int main(int argc, char* argv[]) {
    GstElement *pipeline;
    GstBus *bus;
    GstMessage *msg;

    /* Initialize GStreamer */
    gst_init(&argc, &argv);        
    std::cout << "Dashcam: GStreamer initialized successfully.\n";    

    GError* parse_error = nullptr;
    /* Build Pipeline */
    pipeline = gst_parse_launch(
            "filesrc location=parking-lot.mp4 ! decodebin ! videoconvert ! videoscale "
            "! video/x-raw,format=I420,width=640,height=360,framerate=12/1 "
            //"! tee name=t "
            "! identity sync=true ! tee name=t "

            // Recording branch
            "t. ! queue ! x264enc tune=zerolatency ! h264parse ! mp4mux "
            "! filesink location=recording.mp4 "

            // Frame access branch
            "t. ! queue max-size-buffers=3 max-size-bytes=0 max-size-time=0 leaky=downstream "
            "! identity sleep-time=120000 "
            "! videoconvert ! video/x-raw,format=GRAY8 "
            "! appsink name=frames emit-signals=true sync=false",
            &parse_error);

    if (parse_error || !pipeline) {
        std::cerr << "Pipeline creation failed: "
                  << (parse_error ? parse_error->message: "unknown error") << '\n';
        g_clear_error(&parse_error);
        if (pipeline) gst_object_unref(pipeline);
        
        return 1;
    }
    
     
    std::cout << "Pipeline Created\n";

    MotionState motion{};

    GstElement* frames = gst_bin_get_by_name(GST_BIN(pipeline), "frames");
    g_signal_connect(frames, "new-sample", G_CALLBACK(on_frame), &motion);
    gst_object_unref(frames);

    /* Set state to playing */
    gst_element_set_state(pipeline, GST_STATE_PLAYING);

    /* Error handling */
    bus = gst_element_get_bus(pipeline);
//    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
    const auto message_types = static_cast<GstMessageType>(GST_MESSAGE_ERROR | GST_MESSAGE_EOS);
    msg = gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, message_types);

    /* Error handling */
    if (msg == nullptr) {
        std::cerr << "Bus wait returned no message.\n";
    } else {
        if (GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR) {
            GError* error = nullptr;
            gst_message_parse_error(msg, &error, nullptr);
            std::cerr << "GStreamer error: " << error->message << '\n';
            g_error_free(error);
        } else {
            std::cout << "Reached end of stream.\n";
        }
        gst_message_unref(msg);
    } 

    /* Free resources */
    gst_element_set_state(pipeline, GST_STATE_NULL);
    if (motion.previous) gst_sample_unref(motion.previous);
    gst_object_unref(bus);
    gst_element_set_state(pipeline, GST_STATE_NULL);   
    gst_object_unref(pipeline);

    return 0;
}
