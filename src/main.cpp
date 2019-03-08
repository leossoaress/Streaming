#include "reader.h"
#include <thread>

extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
#include <libavutil/imgutils.h>
}

#define AV_OUTPUT_FORMAT "mpegts"
#define AV_OUTPUT_CODEC  "libx264"
#define AV_OUTPUT_BITRATE 600000000

#define OUTPUT_WIDTH 4096
#define OUTPUT_HEIGHT 2160

typedef struct {
    AVFormatContext *pFmtCtx    = nullptr;
    AVCodecContext  *pCodecCtx  = nullptr;
    AVCodec         *pCodec     = nullptr;
    AVStream        *pStream    = nullptr;
    AVFrame         *pFrame     = nullptr;
    AVProgram       *pProgram   = nullptr;
    AVPacket        *pPacket    = nullptr;
} ff_output_t;

void Erro(const string msg) {
    clog << "Error: " << msg << endl;
    exit(0);
}

void SetCodecParameters(AVCodecContext &ctx) {
    ctx.codec_tag = 0;
    ctx.codec_type = AVMEDIA_TYPE_VIDEO;
    ctx.pix_fmt = AV_PIX_FMT_YUV420P;
    ctx.codec_id = AV_CODEC_ID_H264;
    ctx.bit_rate = AV_OUTPUT_BITRATE;
    ctx.width = OUTPUT_WIDTH;
    ctx.height = OUTPUT_HEIGHT;
    ctx.time_base.den = 21;
    ctx.time_base.num = 1;

    ctx.thread_count = 16;
    ctx.flags |= CODEC_FLAG_LOOP_FILTER;
    ctx.qmin = 10;
    ctx.qmax = 51;
    ctx.qcompress = 0.6;
    ctx.max_qdiff = 4;
    ctx.i_quant_factor = 0.71;
    ctx.me_range = 16;
    ctx.me_cmp = FF_CMP_CHROMA;
    ctx.ildct_cmp = FF_CMP_CHROMA;
    ctx.gop_size = 250;

    ctx.keyint_min = 25;
    ctx.trellis = 0;
    ctx.me_subpel_quality = 0;
    ctx.max_b_frames = 0;
    ctx.refs = 1;
}

int main() {

    av_register_all();
    avformat_network_init();

    Reader reader;
    thread threadCamera = thread(&Reader::Run, &reader);

    int avRet = 0;
    ff_output_t output;
    const char outputName[] = "udp://192.168.11.9:6000?pkt_size=1316";

    //Open output format
    avRet = avformat_alloc_output_context2(&output.pFmtCtx, nullptr, AV_OUTPUT_FORMAT, outputName);
    if(avRet < 0) Erro("Could not create output context");

    //Find encoder
    output.pCodec = avcodec_find_encoder_by_name(AV_OUTPUT_CODEC);
    if(!output.pCodec) Erro("Codec not found");

    //Create and add new stream
    output.pStream = avformat_new_stream(output.pFmtCtx, output.pCodec);
    if(!output.pStream) Erro("Could not create the output stream");

    //Initialize IO context
    if(!(output.pFmtCtx->flags & AVFMT_NOFILE)) {
        avRet = avio_open2(&output.pFmtCtx->pb, outputName, AVIO_FLAG_WRITE, nullptr, nullptr);
        if (avRet < 0) Erro("Could not initialize IO context");
    }

    //Alloc codec context
    output.pCodecCtx = avcodec_alloc_context3(output.pCodec);
    if(!output.pCodecCtx) Erro("Could not alloc codec context");

    //Set codec parameters
    SetCodecParameters(*output.pCodecCtx);

    if(output.pFmtCtx->flags & AVFMT_GLOBALHEADER) {
        output.pCodecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    //Initialize codec stream
    avRet = avcodec_parameters_from_context(output.pStream->codecpar, output.pCodecCtx);
    if(avRet < 0) Erro("Could not initialize stream codec");

    //Set open codec configuration
    AVDictionary *opts = nullptr;
    av_dict_set(&opts, "preset", "ultrafast", 0);
    av_dict_set(&opts, "crf", "22", 0);
    av_dict_set(&opts, "tune", "zerolatency", 0);
    av_dict_set(&opts, "profile", "high", 0);

    av_opt_set(output.pCodecCtx->priv_data, "preset", "ultrafast", 0);
    av_opt_set(output.pCodecCtx->priv_data, "crf", "22", 0);

    //Open video encoder
    avRet = avcodec_open2(output.pCodecCtx, output.pCodec, &opts);
    if(avRet < 0) Erro("Could nt open output codec");

    output.pStream->codecpar->extradata = output.pCodecCtx->extradata;
    output.pStream->codecpar->extradata_size = output.pCodecCtx->extradata_size;

    //Print some information about output
    av_dump_format(output.pFmtCtx, 0, outputName, 1);

    //Initialize converter
    SwsContext *swsctx = sws_alloc_context();
    swsctx = sws_getContext(INPUT_WIDTH, INPUT_HEIGHT, AV_PIX_FMT_UYVY422,
                            OUTPUT_WIDTH, OUTPUT_HEIGHT, AV_PIX_FMT_YUV420P,
                            SWS_BICUBIC,nullptr, nullptr, nullptr);

    //Alloc and initialize frame buffer
    output.pFrame = av_frame_alloc();
    if(!output.pFrame) Erro("Could not alloc frame");

    std::vector<uint8_t> framebuf(av_image_get_buffer_size(output.pCodecCtx->pix_fmt, OUTPUT_WIDTH, OUTPUT_HEIGHT, 1));
    av_image_fill_arrays(output.pFrame->data, output.pFrame->linesize, framebuf.data(), output.pCodecCtx->pix_fmt, OUTPUT_WIDTH, OUTPUT_HEIGHT, 1);

    //Write header
    avRet = avformat_write_header(output.pFmtCtx, &opts);
    if(avRet < 0) Erro("Fail to write outstream header");

    while(true) {

        if(reader.frames.empty()) {
            sem_wait(&reader.writeSemaphore);
        }

        //Convert YUV422 to YUV420P
        const int strideOut[] = {static_cast<int>(reader.GetStride())};
        sws_scale(swsctx, &reader.frames.front(), strideOut, 0, INPUT_HEIGHT, output.pFrame->data, output.pFrame->linesize);
        reader.frames.pop();

        //Set frame config
        output.pFrame->width = OUTPUT_WIDTH;
        output.pFrame->height = OUTPUT_HEIGHT;
        output.pFrame->format = AV_PIX_FMT_YUV420P;
        output.pFrame->pts += av_rescale_q(1, output.pCodecCtx->time_base, output.pStream->time_base);

        //Alloc and initialize packet
        output.pPacket = av_packet_alloc();
        av_init_packet(output.pPacket);

        //Send frame to encoder
        avRet = avcodec_send_frame(output.pCodecCtx, output.pFrame);
        if(avRet < 0) Erro("Error sending frame to codec context");

        //Receive packet from encoder
        avRet = avcodec_receive_packet(output.pCodecCtx, output.pPacket);
        if(avRet < 0) Erro("Error receiving packet from codec context");

        //Write frame
        av_interleaved_write_frame(output.pFmtCtx, output.pPacket);
        av_packet_unref(output.pPacket);
    }

    return 0;
}