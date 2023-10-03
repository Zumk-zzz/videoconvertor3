extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
}
#include <iostream>

int main(int argc, char* argv[]) {
    const char* inputFileName = "..\\videoconvertor\\32.mp4";
    const char* outputFileName = "..\\videoconvertor\\2.avi";
    const char* outputFormat = "avi";

    avformat_network_init();

    AVFormatContext* inputFormatContext = avformat_alloc_context();
    if (!inputFormatContext) {
        std::cerr << "Не удалось выделить контекст для входного файла" << std::endl;
        return 1;
    }

    if (avformat_open_input(&inputFormatContext, inputFileName, nullptr, nullptr) != 0) {
        std::cerr << "Не удалось открыть входной файл" << std::endl;
        return 1;
    }

    if (avformat_find_stream_info(inputFormatContext, nullptr) < 0) {
        std::cerr << "Не удалось найти информацию о потоках во входном файле" << std::endl;
        return 1;
    }

    int videoStreamIndex = -1;
    int audioStreamIndex = -1;

    for (int i = 0; i < inputFormatContext->nb_streams; ++i) {
        AVStream* stream = inputFormatContext->streams[i];
        if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
        }
        else if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioStreamIndex = i;
        }
    }

    if (videoStreamIndex == -1) {
        std::cerr << "Видео-поток не найден во входном файле" << std::endl;
        return 1;
    }

    AVFormatContext* outputFormatContext = nullptr;
    if (avformat_alloc_output_context2(&outputFormatContext, nullptr, outputFormat, outputFileName) < 0) {
        std::cerr << "Не удалось выделить контекст для выходного файла" << std::endl;
        return 1;
    }

    if (avio_open(&outputFormatContext->pb, outputFileName, AVIO_FLAG_WRITE) < 0) {
        std::cerr << "Не удалось открыть выходной файл" << std::endl;
        return 1;
    }

    // Создание выходного видео-потока
    AVStream* outputVideoStream = avformat_new_stream(outputFormatContext, nullptr);
    if (!outputVideoStream) {
        std::cerr << "Не удалось создать выходной видео-поток" << std::endl;
        return 1;
    }

    // Копирование параметров видео из входного контекста
    if (avcodec_parameters_copy(outputVideoStream->codecpar, inputFormatContext->streams[videoStreamIndex]->codecpar) < 0) {
        std::cerr << "Не удалось скопировать параметры видео-потока" << std::endl;
        return 1;
    }

    // Открытие кодека для видео-потока (если необходимо)
    // avcodec_open2(...);

    // Запись заголовка выходного файла после создания потоков
    if (avformat_write_header(outputFormatContext, nullptr) < 0) {
        std::cerr << "Не удалось записать заголовок выходного файла" << std::endl;
        return 1;
    }

    AVPacket packet;
    int64_t lastDts = AV_NOPTS_VALUE; // Инициализируем переменную для отслеживания последней DTS

    while (av_read_frame(inputFormatContext, &packet) >= 0) {
        if (packet.stream_index == videoStreamIndex) {
            if (packet.dts != AV_NOPTS_VALUE) {
                // Проверяем, что DTS монотонно увеличивается
                if (packet.dts < lastDts) {
                    packet.dts = lastDts;
                }
                lastDts = packet.dts;
            }

            // Пересчитываем временные метки
            av_packet_rescale_ts(&packet, inputFormatContext->streams[videoStreamIndex]->time_base, outputVideoStream->time_base);

            if (av_interleaved_write_frame(outputFormatContext, &packet) < 0) {
                std::cerr << "Не удалось записать видео-пакет в выходной файл" << std::endl;
                return 1;
            }
        }

        av_packet_unref(&packet);
    }

    av_write_trailer(outputFormatContext);
    avio_close(outputFormatContext->pb);
    avformat_free_context(outputFormatContext);

    avformat_close_input(&inputFormatContext);
    avformat_free_context(inputFormatContext);

    return 0;
}
