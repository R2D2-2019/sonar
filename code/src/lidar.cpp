#include <lidar.hpp>

namespace r2d2::distance {
    uint8_t lidar_c::receive_uint8() {
        for (;;) {
            // The program gets stuck on uart.available when executed from the process() 
            // function via receive_packet() (in module.hpp,works fine when executed from main.cpp)
            if (uart.available() > 0) { 
                uint8_t byte = uart.receive();
                checksum += byte;
                return byte;
            }
        }
    }

    uint16_t lidar_c::receive_uint16() {
        return (receive_uint8() << 8) | receive_uint8();
    }

    void lidar_c::wait_for_startbyte(uint8_t start_byte) {
        for (;;) {
            if (receive_uint8() == start_byte) {
                checksum = start_byte;
                break;
            }
        }
    }
    

    

    bool lidar_c::receive_packet_header() {
        header.frame_length = receive_uint16();
        header.protocol_version = receive_uint8();
        header.frame_type = receive_uint8();
        header.command_word = receive_uint8();
        header.data_length = receive_uint16();
        header.radar_speed = receive_uint8();

        if (header.frame_type != frame_type_value) {
            // ERROR
            return false;
        }

        if (header.protocol_version == data_protocol_version_value &&
            header.command_word == data_command_word_value) {
            header.zero_offset = receive_uint16();
            header.starting_angle = receive_uint16();
            return true;
        } else if (header.protocol_version == error_protocol_version_value &&
                   header.command_word == error_command_word_value) {
            header.zero_offset = 0xFFFF;
            header.starting_angle = 0xFFFF;
            return true;
        } else {
            // ERROR
            return false;
        }
    }

    void lidar_c::receive_measurement_data() {
        uint8_t measurements_to_receive = ((header.data_length - 5) / 3);

        for (unsigned int count = 0; count < measurements_to_receive; count++) {
            // measurement_angle = starting_angle + (((measurement_count - 1)
            // * 22.5) / total_points_per_frame) This formula is extracted from
            // the datasheet of the lidar communication protocol.
            // 
            // We multiplied everything times 10 in the formula to don't have to use floating point numbers. 
            // At the end we devide by 5 to round at halve degrees so we fill the 720 values in the measurements array to get
            // more accurate measurements.
            measurements[((header.starting_angle / 10) +
                          ((count * (225)) / ((header.data_length - 5) / 3))) /
                         5] = {receive_uint8(), receive_uint16()};
        }
    }

    lidar_c::lidar_c(r2d2::usart::usart_connection_c &uart_conn)
        : measurements({}), uart(uart_conn){};

    bool lidar_c::receive_packet() {
        wait_for_startbyte();
        if (!receive_packet_header() ||
            header.command_word != data_command_word_value) {
            if (header.command_word != error_command_word_value) {
                return false;
            }
            return (checksum == receive_uint16());
        }
        receive_measurement_data();

        return (checksum == receive_uint16());
    }
} // namespace r2d2::distance