#include "pch.h"
#include "FFMuxerObject.h"
#include <windows.h>
#pragma comment(lib,"ws2_32.lib")

FFMuxerObject::FFMuxerObject(const char*szInputH264FileName, const char*szInputAudioFileName, const char*szOutMp4FileName)
	:m_strInputH264FileName(szInputH264FileName)
	,m_strInputAudioFileName(szInputAudioFileName)
	,m_strOutMp4FileName(szOutMp4FileName)
{
	m_ofmt = nullptr;
	m_ifmt_ctx_v = nullptr;
	m_ifmt_ctx_a = nullptr;
	m_ofmt_ctx = nullptr;
	m_aacAbsCtx = nullptr;
}


FFMuxerObject::~FFMuxerObject()
{
	if (m_aacAbsCtx)
	{
		av_bsf_free(&m_aacAbsCtx);
	}
	if (m_ifmt_ctx_v)
	{
		avformat_close_input(&m_ifmt_ctx_v);
	}
	if (m_ifmt_ctx_a)
	{
		avformat_close_input(&m_ifmt_ctx_a);
	}
	if (m_ofmt_ctx && !(m_ofmt_ctx->flags & AVFMT_NOFILE))
	{
		avio_close(m_ofmt_ctx->pb);
		avformat_free_context(m_ofmt_ctx);
	}
}
int FFMuxerObject::get_sr_index(unsigned int sampling_frequency)
{
	switch (sampling_frequency) {
	case 96000: return 0;
	case 88200: return 1;
	case 64000: return 2;
	case 48000: return 3;
	case 44100: return 4;
	case 32000: return 5;
	case 24000: return 6;
	case 22050: return 7;
	case 16000: return 8;
	case 12000: return 9;
	case 11025: return 10;
	case 8000:return 11;
	case 7350:return 12;
	default: return 0;
	}
}

void FFMuxerObject::make_dsi(unsigned int sampling_frequency_index, unsigned int channel_configuration, unsigned char* dsi)
{
	unsigned int object_type = 2; 
	dsi[0] = (object_type << 3) | (sampling_frequency_index >> 1);
	dsi[1] = ((sampling_frequency_index & 1) << 7) | (channel_configuration << 3);
}

bool FFMuxerObject::InitFFmpeg()
{
	int ret = 0;

	if ((ret = avformat_open_input(&m_ifmt_ctx_a, m_strInputAudioFileName.c_str(), NULL, NULL)) != 0)
	{
		printf("Could not open input file.");
		return false;
	}
	if ((ret = avformat_find_stream_info(m_ifmt_ctx_a, 0)) < 0) {
		printf("Failed to retrieve input stream information");
		return false;
	}
	if ((ret = avformat_open_input(&m_ifmt_ctx_v, m_strInputH264FileName.c_str(), NULL, NULL)) != 0) {
		printf("Could not open input file:%d\n", ret);
		return false;
	}
	if ((ret = avformat_find_stream_info(m_ifmt_ctx_v, 0)) < 0) {
		printf("Failed to retrieve input stream information");
		return false;
	}
	avformat_alloc_output_context2(&m_ofmt_ctx, NULL, NULL, m_strOutMp4FileName.c_str());
	if (!m_ofmt_ctx) {
		printf("Could not create output context\n");
		return false;
	}
	m_ofmt = m_ofmt_ctx->oformat;
	
	const AVBitStreamFilter *absFilter = av_bsf_get_by_name("aac_adtstoasc"); //过滤器 增加或者删除adts头
	
	if ((ret = av_bsf_alloc(absFilter, &m_aacAbsCtx)) != 0)
	{
		printf("Could not create av_bsf_alloc\n");
		return false;
	}
	for (auto i = 0; i < m_ifmt_ctx_v->nb_streams; i++) {
		//Create output AVStream according to input AVStream
		if (m_ifmt_ctx_v->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			AVStream *in_stream = m_ifmt_ctx_v->streams[i];
			AVCodec *avcodec = avcodec_find_decoder(in_stream->codecpar->codec_id);
			AVStream *out_stream = avformat_new_stream(m_ofmt_ctx, avcodec);
			videoindex_input = i;
			if (!out_stream) {
				printf("Failed allocating output stream\n");
				return false;
			}
			videoindex_out = out_stream->index;
			AVCodecContext *codec_ctx = avcodec_alloc_context3(avcodec);
			ret = avcodec_parameters_to_context(codec_ctx, in_stream->codecpar);
			if (ret < 0) {
				printf("Failed to copy context from input to output stream codec context\n");
				return false;
			}
			codec_ctx->codec_tag = 0;
			if (m_ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

			ret = avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
			int q = 0;
		}
	}

	for (auto i = 0; i < m_ifmt_ctx_a->nb_streams; i++) {
		if (m_ifmt_ctx_a->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			AVStream *in_stream = m_ifmt_ctx_a->streams[i];
			AVCodec *avcodec = avcodec_find_decoder(in_stream->codecpar->codec_id);
			AVStream *out_stream = avformat_new_stream(m_ofmt_ctx, avcodec);
			audioindex_input = i;
			if (!out_stream) {
				printf("Failed allocating output stream\n");
				ret = AVERROR_UNKNOWN;
				return false;
			}
			audioindex_out = out_stream->index;
			avcodec = avcodec_find_decoder(in_stream->codecpar->codec_id);
			avcodec_parameters_copy(m_aacAbsCtx->par_in, in_stream->codecpar);
			av_bsf_init(m_aacAbsCtx);

			AVCodecContext *codec_ctx = avcodec_alloc_context3(avcodec);
			ret = avcodec_parameters_to_context(codec_ctx, in_stream->codecpar);
			if (ret < 0) {
				printf("Failed to copy context from input to output stream codec context\n");
				return false;
			}
			codec_ctx->codec_tag = 0;
			if (m_ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
				codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

			ret = avcodec_parameters_from_context(out_stream->codecpar, codec_ctx);
			out_stream->codecpar->extradata = (uint8_t*)av_malloc(2);
			out_stream->codecpar->extradata_size = 2;
			unsigned char dsi1[2];
			unsigned int sampling_frequency_index = (unsigned int)get_sr_index(8000);
			make_dsi(sampling_frequency_index, 1, dsi1);
			memcpy(out_stream->codecpar->extradata, dsi1, 2);
			break;
		}
	}
	if (!(m_ofmt->flags & AVFMT_NOFILE))
	{
		if (avio_open(&m_ofmt_ctx->pb, m_strOutMp4FileName.c_str(), AVIO_FLAG_WRITE) < 0)
		{
			printf("Could not open output file '%s'", m_strOutMp4FileName.c_str());
			return false;
		}
	}

	AVDictionary *movflags = nullptr;
	ret = avformat_write_header(m_ofmt_ctx, &movflags);
	if (ret < 0) {
		fprintf(stderr, "Error occurred when opening output file\n");
		return false;
	}

	return true;
}

void FFMuxerObject::Muxering()
{
	int64_t cur_pts_v = 0, cur_pts_a = 0;
	int frame_index = 1;
	while (1) {
		AVFormatContext *ifmt_ctx;
		int stream_index = 0;
		AVStream *in_stream, *out_stream, *add_out_stream;
		int compare_tag = -1;
		
		compare_tag = av_compare_ts(cur_pts_v, m_ifmt_ctx_v->streams[videoindex_input]->time_base, cur_pts_a, m_ifmt_ctx_a->streams[audioindex_input]->time_base);
		

		if (compare_tag <= 0) {
			ifmt_ctx = m_ifmt_ctx_v;
			stream_index = videoindex_out;

			if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
				do {
					in_stream = ifmt_ctx->streams[pkt.stream_index];
					out_stream = m_ofmt_ctx->streams[stream_index];

					if (pkt.stream_index == videoindex_input) {
						//FIX£∫No PTS (Example: Raw H.264)
						//Simple Write PTS
						if (pkt.pts == AV_NOPTS_VALUE) {
							//Write PTS
							AVRational time_base1 = in_stream->time_base;
							//Duration between 2 frames (us)
							int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
							//Parameters
							pkt.pts = (double)(frame_index*calc_duration) / (double)(av_q2d(time_base1)*AV_TIME_BASE);
							pkt.dts = pkt.pts;
							pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1)*AV_TIME_BASE);
							frame_index++;
						}

						cur_pts_v = pkt.pts;
						break;
					}
				} while (av_read_frame(ifmt_ctx, &pkt) >= 0);
			}
			else {
				break;
			}
		}
		else {
			ifmt_ctx = m_ifmt_ctx_a;
			stream_index = audioindex_out;
			if (av_read_frame(ifmt_ctx, &pkt) >= 0) {
				do {
					in_stream = ifmt_ctx->streams[pkt.stream_index];
					out_stream = m_ofmt_ctx->streams[stream_index];
					if (pkt.stream_index == audioindex_input) {

						//FIX£∫No PTS
						//Simple Write PTS
						if (pkt.pts == AV_NOPTS_VALUE) {
							//Write PTS
							AVRational time_base1 = in_stream->time_base;
							//Duration between 2 frames (us)
							int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
							//Parameters
							pkt.pts = (double)(frame_index*calc_duration) / (double)(av_q2d(time_base1)*AV_TIME_BASE);
							pkt.dts = pkt.pts;
							pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1)*AV_TIME_BASE);
							frame_index++;
						}
						cur_pts_a = pkt.pts;

						break;
					}
				} while (av_read_frame(ifmt_ctx, &pkt) >= 0);
			}
			else {
				break;
			}

		}

		if (1 == stream_index)
		{
			if (av_bsf_send_packet(m_aacAbsCtx, &pkt) != 0) {
				av_packet_unref(&pkt);
				continue;
			}
			while (av_bsf_receive_packet(m_aacAbsCtx, &pkt) == 0) {

				//Convert PTS/DTS
				
				pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
				pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

				pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);

				pkt.flags = 1;
				pkt.stream_index = stream_index;
				int ret = 0;
				if ((ret = av_interleaved_write_frame(m_ofmt_ctx, &pkt)) < 0) {
					printf("Error muxing packet\n");
					break;
				}

				av_packet_unref(&pkt);
				continue;
			}
		}
		else {
			unsigned char* naluData = (unsigned char*)pkt.data;
			int naluSize = (int)pkt.size;

			const int iStartCodeLen = (0x00000001 == ::ntohl(*(DWORD*)naluData)) ? 4 : 3;
			const int iNALUType = naluData[iStartCodeLen] & 0x1f;
			if (iNALUType == 7)
			{
				int a = 0;
			}
			if (pkt.pts == 0)
			{
				pkt.pts = 1;
			}
			if (pkt.dts == 0)
			{
				pkt.dts = 1;
			}
			pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
			pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

			pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
			pkt.pos = -1;
			pkt.stream_index = stream_index;

			int ret = 0;
			if ((ret = av_interleaved_write_frame(m_ofmt_ctx, &pkt)) < 0) {
				printf("Error muxing packet\n");
				break;
			}
			av_packet_unref(&pkt);
		}
	}
	//Write file trailer
	av_write_trailer(m_ofmt_ctx);
}
