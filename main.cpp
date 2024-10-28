#include <iostream>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>

void transpose_frame(AVFrame* frame) {
    int width = frame->width;
    int height = frame->height;
    uint8_t* src_data[4] = { frame->data[0], frame->data[1], frame->data[2], frame->data[3] };
    int src_linesize[4] = { frame->linesize[0], frame->linesize[1], frame->linesize[2], frame->linesize[3] };

    // Create a new frame for the transposed output
    AVFrame* transposed_frame = av_frame_alloc();
    transposed_frame->format = frame->format;
    transposed_frame->width = height;
    transposed_frame->height = width;

    // Allocate buffer for the new frame
    av_image_alloc(transposed_frame->data, transposed_frame->linesize, transposed_frame->width, transposed_frame->height, (AVPixelFormat)frame->format, 1);

    // Transpose (rotate 90 degrees clockwise)
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            int src_index = y * frame->linesize[0] + x * 3;      // RGB pixel size = 3
            int dst_index = x * transposed_frame->linesize[0] + (height - y - 1) * 3;
            
            transposed_frame->data[0][dst_index + 0] = frame->data[0][src_index + 0]; // Red
            transposed_frame->data[0][dst_index + 1] = frame->data[0][src_index + 1]; // Green
            transposed_frame->data[0][dst_index + 2] = frame->data[0][src_index + 2]; // Blue
        }
    }

    // Free the original frame data and set it to the transposed frame
    av_freep(&frame->data[0]);
    frame->data[0] = transposed_frame->data[0];
    frame->linesize[0] = transposed_frame->linesize[0];
    frame->width = transposed_frame->width;
    frame->height = transposed_frame->height;

    // Free transposed_frame structure (without freeing data, as it now belongs to the main frame)
    av_frame_free(&transposed_frame);
}

int main(int argc, char* argv[]) {
//    av_register_all();
//    avcodec_register_all();

    const char* input_file = "input.mp4";
    const char* output_file = "output_transposed.mp4";

    // Open input and output files
    AVFormatContext* input_format_ctx = nullptr;
    avformat_open_input(&input_format_ctx, input_file, nullptr, nullptr);
    avformat_find_stream_info(input_format_ctx, nullptr);

    AVFormatContext* output_format_ctx = nullptr;
    avformat_alloc_output_context2(&output_format_ctx, nullptr, nullptr, output_file);

    // Find video stream index
    int video_stream_index = -1;
    for (int i = 0; i < input_format_ctx->nb_streams; ++i) {
        if (input_format_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            video_stream_index = i;
            break;
        }
    }

    // Copy video stream codec parameters
    AVStream* video_stream = input_format_ctx->streams[video_stream_index];
  const AVCodec* codec = avcodec_find_decoder(video_stream->codecpar->codec_id);
    AVCodecContext* codec_ctx = avcodec_alloc_context3(codec);
    avcodec_parameters_to_context(codec_ctx, video_stream->codecpar);
    avcodec_open2(codec_ctx, codec, nullptr);

    // Encoding
   const 
   AVCodec* encoder = avcodec_find_encoder(codec_ctx->codec_id);
    AVCodecContext* encoder_ctx = avcodec_alloc_context3(encoder);
    encoder_ctx->width = codec_ctx->height;
    encoder_ctx->height = codec_ctx->width;
    encoder_ctx->pix_fmt = codec_ctx->pix_fmt;
    encoder_ctx->time_base = video_stream->time_base;
    avcodec_open2(encoder_ctx, encoder, nullptr);

    // Create new output stream and copy codec parameters
    AVStream* out_stream = avformat_new_stream(output_format_ctx, encoder);
    avcodec_parameters_from_context(out_stream->codecpar, encoder_ctx);

    avio_open(&output_format_ctx->pb, output_file, AVIO_FLAG_WRITE);
    int ret = avformat_write_header(output_format_ctx, nullptr);
    if (ret < 0) {
        std::cerr << "Error occurred while writing header: " << av_err2str(ret) << std::endl;
        // Handle the error appropriately (e.g., cleanup and return an error code)
        return ret;
    }

    // Read, process, and encode each frame
   AVPacket* packet = av_packet_alloc();
if (!packet) {
    std::cerr << "Could not allocate AVPacket" << std::endl;
    return -1;  // Handle error appropriately
}

    AVFrame* frame = av_frame_alloc();
    while (av_read_frame(input_format_ctx, packet) >= 0) {
    if (packet->stream_index == video_stream_index) {
        avcodec_send_packet(codec_ctx, packet);
        if (avcodec_receive_frame(codec_ctx, frame) == 0) {
           
                transpose_frame(frame);
                avcodec_send_frame(encoder_ctx, frame);
               AVPacket* out_packet = av_packet_alloc();
                if (!out_packet) {
                    std::cerr << "Could not allocate AVPacket" << std::endl;
                    return -1;  // Handle error appropriately
                }

                if (avcodec_receive_packet(encoder_ctx, out_packet) == 0) {
                    out_packet->stream_index = out_stream->index;
                    av_write_frame(output_format_ctx, out_packet);
                    av_packet_unref(out_packet);  // Unref packet to reuse it
                }

                // Free the allocated packet when done
                av_packet_free(&out_packet);
            }
        }
        av_packet_unref(packet);
    }

    // Cleanup
    av_write_trailer(output_format_ctx);
    avcodec_close(codec_ctx);
    avcodec_close(encoder_ctx);
    avformat_close_input(&input_format_ctx);
    avio_close(output_format_ctx->pb);
    avformat_free_context(output_format_ctx);
    av_frame_free(&frame);

    std::cout << "Transposed video saved as " << output_file << std::endl;
    return 0;
}
