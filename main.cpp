#include <stdio.h>
#include <stdlib.h>
extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libavutil/imgutils.h>
    #include <libswscale/swscale.h>
}

class VideoTranspose {
private:
    AVFormatContext *inputFormatContext = nullptr;
    AVFormatContext *outputFormatContext = nullptr;
    AVCodecContext *inputCodecContext = nullptr;
    AVCodecContext *outputCodecContext = nullptr;
    AVStream *inputVideoStream = nullptr;
    AVStream *outputVideoStream = nullptr;
    int videoStreamIndex = -1;
    AVFrame *frame = nullptr;
    AVFrame *transposedFrame = nullptr;
    AVPacket *packet = nullptr;

    void transpose90Degrees(AVFrame *src, AVFrame *dst) {
        for (int plane = 0; plane < 3; plane++) {
            uint8_t *srcData = src->data[plane];
            uint8_t *dstData = dst->data[plane];
            int srcLinesize = src->linesize[plane];
            int dstLinesize = dst->linesize[plane];
            
            int width = plane == 0 ? src->width : src->width / 2;
            int height = plane == 0 ? src->height : src->height / 2;
            
            for (int y = 0; y < height; y++) {
                for (int x = 0; x < width; x++) {
                    dstData[x * dstLinesize + (height - 1 - y)] = 
                        srcData[y * srcLinesize + x];
                }
            }
        }
    }

public:
    ~VideoTranspose() {
        if (frame) av_frame_free(&frame);
        if (transposedFrame) av_frame_free(&transposedFrame);
        if (packet) av_packet_free(&packet);
        if (inputCodecContext) avcodec_free_context(&inputCodecContext);
        if (outputCodecContext) avcodec_free_context(&outputCodecContext);
        if (inputFormatContext) avformat_close_input(&inputFormatContext);
        if (outputFormatContext) {
            if (!(outputFormatContext->flags & AVFMT_NOFILE))
                avio_closep(&outputFormatContext->pb);
            avformat_free_context(outputFormatContext);
        }
    }

    bool openInput(const char* inputFile) {
        if (avformat_open_input(&inputFormatContext, inputFile, nullptr, nullptr) < 0) {
            fprintf(stderr, "Could not open input file\n");
            return false;
        }

        if (avformat_find_stream_info(inputFormatContext, nullptr) < 0) {
            fprintf(stderr, "Could not find stream info\n");
            return false;
        }

        // Find video stream
        for (unsigned int i = 0; i < inputFormatContext->nb_streams; i++) {
            if (inputFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIndex = i;
                break;
            }
        }

        if (videoStreamIndex == -1) {
            fprintf(stderr, "Could not find video stream\n");
            return false;
        }

        inputVideoStream = inputFormatContext->streams[videoStreamIndex];
        
        // Find decoder
        const AVCodec *decoder = avcodec_find_decoder(inputVideoStream->codecpar->codec_id);
        if (!decoder) {
            fprintf(stderr, "Could not find decoder\n");
            return false;
        }

        // Allocate codec context
        inputCodecContext = avcodec_alloc_context3(decoder);
        if (!inputCodecContext) {
            fprintf(stderr, "Could not allocate decoder context\n");
            return false;
        }

        if (avcodec_parameters_to_context(inputCodecContext, inputVideoStream->codecpar) < 0) {
            fprintf(stderr, "Could not copy codec params to codec context\n");
            return false;
        }

        if (avcodec_open2(inputCodecContext, decoder, nullptr) < 0) {
            fprintf(stderr, "Could not open codec\n");
            return false;
        }

        return true;
    }

    bool setupOutput(const char* outputFile) {
        avformat_alloc_output_context2(&outputFormatContext, nullptr, nullptr, outputFile);
        if (!outputFormatContext) {
            fprintf(stderr, "Could not create output context\n");
            return false;
        }

        // Create output video stream
        const AVCodec *encoder = avcodec_find_encoder(AV_CODEC_ID_H264);
        if (!encoder) {
            fprintf(stderr, "Could not find encoder\n");
            return false;
        }

        outputVideoStream = avformat_new_stream(outputFormatContext, nullptr);
        if (!outputVideoStream) {
            fprintf(stderr, "Could not create output stream\n");
            return false;
        }

        outputCodecContext = avcodec_alloc_context3(encoder);
        if (!outputCodecContext) {
            fprintf(stderr, "Could not allocate encoder context\n");
            return false;
        }

        // Set output parameters (note: height and width are swapped for 90-degree rotation)
        outputCodecContext->height = inputCodecContext->width;
        outputCodecContext->width = inputCodecContext->height;
        outputCodecContext->pix_fmt = AV_PIX_FMT_YUV420P;
        outputCodecContext->bit_rate = 2000000;
        outputCodecContext->time_base = (AVRational){1, 25};
        outputCodecContext->framerate = (AVRational){25, 1};
        outputCodecContext->gop_size = 10;
        outputCodecContext->max_b_frames = 1;

        if (outputFormatContext->oformat->flags & AVFMT_GLOBALHEADER)
            outputCodecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

        if (avcodec_open2(outputCodecContext, encoder, nullptr) < 0) {
            fprintf(stderr, "Could not open encoder\n");
            return false;
        }

        if (avcodec_parameters_from_context(outputVideoStream->codecpar, outputCodecContext) < 0) {
            fprintf(stderr, "Could not copy encoder parameters to output stream\n");
            return false;
        }

        if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
            if (avio_open(&outputFormatContext->pb, outputFile, AVIO_FLAG_WRITE) < 0) {
                fprintf(stderr, "Could not open output file\n");
                return false;
            }
        }

        if (avformat_write_header(outputFormatContext, nullptr) < 0) {
            fprintf(stderr, "Error writing header\n");
            return false;
        }

        return true;
    }

    bool process() {
        frame = av_frame_alloc();
        transposedFrame = av_frame_alloc();
        packet = av_packet_alloc();
        
        if (!frame || !transposedFrame || !packet) {
            fprintf(stderr, "Could not allocate frame or packet\n");
            return false;
        }

        // Allocate transposed frame buffer
        transposedFrame->format = outputCodecContext->pix_fmt;
        transposedFrame->width = outputCodecContext->width;
        transposedFrame->height = outputCodecContext->height;
        if (av_frame_get_buffer(transposedFrame, 0) < 0) {
            fprintf(stderr, "Could not allocate transposed frame buffer\n");
            return false;
        }

        while (av_read_frame(inputFormatContext, packet) >= 0) {
            if (packet->stream_index == videoStreamIndex) {
                int response = avcodec_send_packet(inputCodecContext, packet);
                if (response < 0) {
                    fprintf(stderr, "Error sending packet for decoding\n");
                    return false;
                }

                while (response >= 0) {
                    response = avcodec_receive_frame(inputCodecContext, frame);
                    if (response == AVERROR(EAGAIN) || response == AVERROR_EOF)
                        break;
                    else if (response < 0) {
                        fprintf(stderr, "Error receiving frame\n");
                        return false;
                    }

                    // Transpose frame
                    transpose90Degrees(frame, transposedFrame);
                    transposedFrame->pts = frame->pts;

                    // Encode transposed frame
                    if (avcodec_send_frame(outputCodecContext, transposedFrame) < 0) {
                        fprintf(stderr, "Error sending frame for encoding\n");
                        return false;
                    }

                    while (true) {
                        AVPacket *outPacket = av_packet_alloc();
                        response = avcodec_receive_packet(outputCodecContext, outPacket);
                        if (response == AVERROR(EAGAIN) || response == AVERROR_EOF) {
                            av_packet_free(&outPacket);
                            break;
                        } else if (response < 0) {
                            fprintf(stderr, "Error receiving packet from encoder\n");
                            av_packet_free(&outPacket);
                            return false;
                        }

                        outPacket->stream_index = 0;
                        av_packet_rescale_ts(outPacket, outputCodecContext->time_base, 
                                           outputVideoStream->time_base);

                        response = av_interleaved_write_frame(outputFormatContext, outPacket);
                        av_packet_free(&outPacket);
                        if (response < 0) {
                            fprintf(stderr, "Error writing frame\n");
                            return false;
                        }
                    }
                }
            }
            av_packet_unref(packet);
        }

        // Write trailer
        av_write_trailer(outputFormatContext);
        return true;
    }
};

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <input_file> <output_file>\n", argv[0]);
        return 1;
    }

    VideoTranspose transposer;
    
    if (!transposer.openInput(argv[1])) {
        fprintf(stderr, "Failed to open input\n");
        return 1;
    }

    if (!transposer.setupOutput(argv[2])) {
        fprintf(stderr, "Failed to setup output\n");
        return 1;
    }

    if (!transposer.process()) {
        fprintf(stderr, "Failed to process video\n");
        return 1;
    }

    printf("Video processing completed successfully\n");
    return 0;
}