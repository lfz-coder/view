/**
 * @file viewAVTrans.cc
 * @brief HLS 转码模块实现 —— M3U8Info 解析/生成 / HLSTransCoder
 */

#include "viewAVTrans.h"
#include "viewUtil.h"
#include "viewLog.h"

namespace viewAVTrans {
    M3U8Info::M3U8Info(const std::string& fileName) : _fileName(fileName) {}

    bool M3U8Info::Parse() {
        std::string content;
        bool ret = viewUtil::FileUtil::Read(_fileName, content);
        if(!ret) {
            viewLog::ERROR("M3U8Info:Parse read file failed: {}", _fileName);
            return false; // 读取文件失败
        }
        // 解析内容
        std::vector<std::string> lines;
        viewUtil::StrUtil::Split(content, "\n", lines);
        for(size_t i = 0; i < lines.size(); i++) {
            if(lines[i].empty()) continue;
            
            if(lines[i].find(HLS_ENDLIST) != std::string::npos) {
                _headers.push_back(lines[i]);  // 也保留 ENDLIST 行
                break;
            }
            
            if(lines[i].find(HLS_EXTINF) != std::string::npos) {
                // 检查是否有下一行
                if(i + 1 >= lines.size()) {
                    viewLog::WARN("M3U8Info:Parse: EXTINF without URL");
                    break;
                }
                _urlPairs.push_back({lines[i], lines[i+1]});
                i++;  // 跳过 URL 行
                continue;
            }
            
            _headers.push_back(lines[i]);
        }

        return true;
    }

    bool M3U8Info::Write() {
        std::stringstream ss;
        for(const auto& header : _headers) {
            ss << header << "\n";
        }
        for(const auto& pair : _urlPairs) {
            ss << pair.first << "\n" << pair.second << "\n";
        }
        bool ret = viewUtil::FileUtil::Write(_fileName, ss.str());
        if(!ret) {
            viewLog::ERROR("M3U8Info:Write write file failed: {}", _fileName);
            return false; // 写入文件失败
        }
        return true;
    }

    std::vector<std::string>& M3U8Info::GetHeaders() {
        return _headers;
    }

    std::vector<M3U8Info::StrPair>& M3U8Info::GetUrlPairs() {
        return _urlPairs;
    }

    // *********************************************************************************************** //

    HLSTransCoder::HLSTransCoder(const HLSConfig& config) : _config(config) {}

    bool HLSTransCoder::Transcode(const std::string& inputFile, const std::string& outputFile) {
        // 所有变量在函数开头定义，确保 goto cleanup 不会跳过初始化
        int ret = 0;
        AVFormatContext* inputContext  = nullptr;
        AVFormatContext* outputContext = nullptr;
        AVDictionary*    options       = nullptr;
        std::vector<int> stream_mapping;
        bool             header_written = false;
        bool             avio_opened    = false;
        int              frame_count    = 0;

        NetworkGuard networkGuard;
        av_log_set_level(AV_LOG_WARNING); // 使用 WARNING 级别，保留关键错误信息

        // 步骤1: 打开输入文件
        ret = avformat_open_input(&inputContext, inputFile.c_str(), nullptr, nullptr);
        if (ret < 0) {
            viewLog::ERROR("HLSTransCoder::Transcode: {}: {}", inputFile, AvError(ret));
            goto cleanup;
        }
        viewLog::INFO("HLSTransCoder::Transcode: {}: open file", inputFile);

        // 步骤2: 解析流信息
        ret = avformat_find_stream_info(inputContext, nullptr);
        if (ret < 0) {
            viewLog::ERROR("HLSTransCoder::Transcode: {}: {}", inputFile, AvError(ret));
            goto cleanup;
        }
        viewLog::INFO("HLSTransCoder::Transcode: {}: 时长: {:.2f}s, 流数量: {}",
                      inputFile, (inputContext->duration / (double)AV_TIME_BASE),
                      inputContext->nb_streams);

        // 步骤3: 创建 HLS 输出上下文
        ret = avformat_alloc_output_context2(&outputContext, nullptr, "hls", outputFile.c_str());
        if (ret < 0 || !outputContext) {
            viewLog::ERROR("HLSTransCoder::Transcode: {}: 创建输出上下文失败", outputFile);
            goto cleanup;
        }

        // 步骤4: 创建输出流并复制编码参数
        stream_mapping.resize(inputContext->nb_streams, -1);
        for (unsigned int i = 0; i < inputContext->nb_streams; i++) {
            AVStream* outStream = avformat_new_stream(outputContext, nullptr);
            if (!outStream) {
                viewLog::ERROR("HLSTransCoder::Transcode: {}: 创建输出流失败 (索引: {})",
                               outputFile, i);
                goto cleanup;
            }
            ret = avcodec_parameters_copy(outStream->codecpar,
                                          inputContext->streams[i]->codecpar);
            if (ret < 0) {
                viewLog::ERROR("HLSTransCoder::Transcode: {}: 复制编码参数失败 (索引: {}): {}",
                               outputFile, i, AvError(ret));
                goto cleanup;
            }
            stream_mapping[i] = outStream->index;
        }

        // 步骤5: 配置 HLS 参数
        av_dict_set_int(&options, "hls_time", _config.hls_time, 0);
        if (!_config.hls_base_url.empty()) {
            av_dict_set(&options, "hls_base_url", _config.hls_base_url.c_str(), 0);
        }
        av_dict_set(&options, "hls_playlist_type", _config.hls_playlist_type.c_str(), 0);
        viewLog::INFO("HLSTransCoder::Transcode: {}: HLS 配置: 分片={}s, 类型={}",
                      outputFile, _config.hls_time, _config.hls_playlist_type);

        // 步骤6: 打开输出文件并写入头信息
        if (!(outputContext->oformat->flags & AVFMT_NOFILE)) {
            ret = avio_open2(&outputContext->pb, outputContext->url,
                             AVIO_FLAG_WRITE, nullptr, nullptr);
            if (ret < 0) {
                viewLog::ERROR("HLSTransCoder::Transcode: {}: 打开输出文件失败", outputFile);
                goto cleanup;
            }
            avio_opened = true;
        }

        ret = avformat_write_header(outputContext, &options);
        if (ret < 0) {
            viewLog::ERROR("HLSTransCoder::Transcode: {}: 写入文件头失败", outputFile);
            goto cleanup;
        }
        header_written = true;
        viewLog::INFO("HLSTransCoder::Transcode: {}: 开始转换...", outputFile);

        // 步骤7: 读取并写入数据包（核心循环）
        {
            AVPacket packet;
            while (av_read_frame(inputContext, &packet) >= 0) {
                unsigned int in_index = packet.stream_index;
                if (in_index >= stream_mapping.size()) {
                    av_packet_unref(&packet);
                    continue;
                }
                int out_index = stream_mapping[in_index];
                if (out_index < 0) {
                    av_packet_unref(&packet);
                    continue;
                }

                AVStream* inStream  = inputContext->streams[in_index];
                AVStream* outStream = outputContext->streams[out_index];

                // 时间戳转换
                if (packet.pts != AV_NOPTS_VALUE) {
                    packet.pts = av_rescale_q_rnd(packet.pts, inStream->time_base,
                                                  outStream->time_base,
                                                  (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                }
                if (packet.dts != AV_NOPTS_VALUE) {
                    packet.dts = av_rescale_q_rnd(packet.dts, inStream->time_base,
                                                  outStream->time_base,
                                                  (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));
                }
                if (packet.duration > 0) {
                    packet.duration = av_rescale_q(packet.duration, inStream->time_base,
                                                   outStream->time_base);
                }
                packet.pos = -1;
                packet.stream_index = out_index;

                ret = av_interleaved_write_frame(outputContext, &packet);
                if (ret < 0) {
                    viewLog::ERROR("HLSTransCoder::Transcode: {}: 写入包失败 (帧 {}): {}",
                                   outputFile, frame_count, AvError(ret));
                    av_packet_unref(&packet);
                    goto cleanup;
                }
                av_packet_unref(&packet);
                ++frame_count;
            }
        }
        viewLog::INFO("HLSTransCoder::Transcode: {}: 数据处理完成，共 {} 个包", outputFile, frame_count);

        // 步骤8: 写入文件尾
        ret = av_write_trailer(outputContext);
        if (ret < 0) {
            viewLog::ERROR("HLSTransCoder::Transcode: {}: 写入文件尾失败", outputFile);
            goto cleanup;
        }
        viewLog::INFO("HLSTransCoder::Transcode: {}: 转换完成！", outputFile);

    cleanup:
        // 统一的资源清理路径
        av_dict_free(&options);
        if (outputContext) {
            if (header_written) {
                // 如果已经写入头部，尝试写入尾部（忽略错误，尽力而为）
                av_write_trailer(outputContext);
            }
            if (avio_opened && outputContext->pb) {
                avio_closep(&outputContext->pb);
            }
            avformat_free_context(outputContext);
        }
        if (inputContext) {
            avformat_close_input(&inputContext);
        }

        return (ret >= 0);
    }

    const char *HLSTransCoder::AvError(int errnum) {
        thread_local char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(errnum, errbuf, sizeof(errbuf));
        return errbuf;
    }
}