#include <climits>
#include <iomanip>
#include <iostream>
#include <type_traits>

#include "../libsflow/libsflow.hpp"
#include "sflow_collector.hpp"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2ipdef.h> // sockaddr_in6
#include <ws2tcpip.h> // socklen_t
#else
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "../fast_library.hpp"
#include "../fastnetmon_plugin.hpp"

#include "../all_logcpp_libraries.hpp"

extern log4cpp::Category& logger;

#include "../simple_packet_parser_ng.hpp"

#include <boost/algorithm/string.hpp>

#include "../fastnetmon_configuration_scheme.hpp"

extern fastnetmon_configuration_t fastnetmon_global_configuration;

std::string raw_udp_packets_received_desc = "Number of raw packets received without any errors";
uint64_t raw_udp_packets_received         = 0;

std::string udp_receive_errors_desc = "Number of failed receives";
uint64_t udp_receive_errors         = 0;

std::string udp_receive_eagain_desc = "Number of eagains";
uint64_t udp_receive_eagain         = 0;

std::string plugin_name       = "sflow";
std::string plugin_log_prefix = plugin_name + ": ";

std::string sflow_total_packets_desc = "Total number of received UDP sFlow packets";
uint64_t sflow_total_packets         = 0;

std::string sflow_bad_packets_desc = "Incorrectly crafted sFlow packets";
uint64_t sflow_bad_packets         = 0;

std::string sflow_flow_samples_desc = "Number of flow samples, i.e. with packet headers";
uint64_t sflow_flow_samples         = 0;

std::string sflow_bad_flow_samples_desc = "Number of broken flow samples";
uint64_t sflow_bad_flow_samples         = 0;

std::string sflow_with_padding_at_the_end_of_packet_desc =
    "Number of packets where we have padding at the end of packet";
uint64_t sflow_with_padding_at_the_end_of_packet = 0;

std::string sflow_padding_flow_sample_desc = "Number of packets with padding inside flow sample";
uint64_t sflow_padding_flow_sample         = 0;

std::string sflow_parse_error_nested_header_desc =
    "Number of packet headers from flow samples which could not be decoded correctly";
uint64_t sflow_parse_error_nested_header = 0;

std::string sflow_counter_sample_desc = "Number of counter samples, i.e. with port counters";
uint64_t sflow_counter_sample         = 0;

std::string sflow_expanded_counter_sample_desc = "Number of expanded counter samples, i.e. with port counters";
uint64_t sflow_expanded_counter_sample         = 0;

std::string sflow_generic_interface_counter_sample_desc =
    "Number of counter samples with generic interface counter information";
uint64_t sflow_generic_interface_counter_sample = 0;

std::string sflow_raw_packet_headers_total_desc = "Number of packet headers from flow samples";
uint64_t sflow_raw_packet_headers_total         = 0;

std::string sflow_extended_router_data_records_desc = "Number of records with extended information from routers";
uint64_t sflow_extended_router_data_records         = 0;

std::string sflow_extended_switch_data_records_desc = "Number of samples with switch data";
uint64_t sflow_extended_switch_data_records         = 0;

std::string sflow_extended_gateway_data_records_desc = "Number of samples with gateway data";
uint64_t sflow_extended_gateway_data_records         = 0;

std::string sflow_unknown_header_protocol_desc = "Number of packets for unknown header protocol";
uint64_t sflow_unknown_header_protocol         = 0;

std::string sflow_ipv4_header_protocol_desc = "Number of samples with IPv4 packet headers";
uint64_t sflow_ipv4_header_protocol         = 0;

std::string sflow_ipv6_header_protocol_desc = "Number of samples with IPv6 packet headers";
uint64_t sflow_ipv6_header_protocol         = 0;

std::string sflow_packets_discarded_desc = "Number of packets discarded by device";
uint64_t sflow_packets_discarded         = 0;

std::vector<system_counter_t> get_sflow_stats() {
    std::vector<system_counter_t> counters;

    counters.push_back(system_counter_t("sflow_raw_udp_packets_received", raw_udp_packets_received,
                                        metric_type_t::counter, raw_udp_packets_received_desc));
    counters.push_back(system_counter_t("sflow_udp_receive_errors", udp_receive_errors, metric_type_t::counter, udp_receive_errors_desc));
    counters.push_back(system_counter_t("sflow_udp_receive_eagain", udp_receive_eagain, metric_type_t::counter, udp_receive_eagain_desc));
    counters.push_back(system_counter_t("sflow_total_packets", sflow_total_packets, metric_type_t::counter, sflow_total_packets_desc));
    counters.push_back(system_counter_t("sflow_bad_packets", sflow_bad_packets, metric_type_t::counter, sflow_bad_packets_desc));
    counters.push_back(system_counter_t("sflow_flow_samples", sflow_flow_samples, metric_type_t::counter, sflow_flow_samples_desc));
    counters.push_back(system_counter_t("sflow_bad_flow_samples", sflow_bad_flow_samples, metric_type_t::counter,
                                        sflow_bad_flow_samples_desc));
    counters.push_back(system_counter_t("sflow_padding_flow_sample", sflow_padding_flow_sample, metric_type_t::counter,
                                        sflow_padding_flow_sample_desc));
    counters.push_back(system_counter_t("sflow_with_padding_at_the_end_of_packet", sflow_with_padding_at_the_end_of_packet,
                                        metric_type_t::counter, sflow_with_padding_at_the_end_of_packet_desc));
    counters.push_back(system_counter_t("sflow_parse_error_nested_header", sflow_parse_error_nested_header,
                                        metric_type_t::counter, sflow_parse_error_nested_header_desc));
    counters.push_back(system_counter_t("sflow_counter_sample", sflow_counter_sample, metric_type_t::counter, sflow_counter_sample_desc));
    
    counters.push_back(system_counter_t("sflow_expanded_counter_sample", sflow_expanded_counter_sample,
                                        metric_type_t::counter, sflow_expanded_counter_sample_desc));

    counters.push_back(system_counter_t("sflow_generic_interface_counter_sample", sflow_generic_interface_counter_sample,
                                        metric_type_t::counter, sflow_generic_interface_counter_sample_desc));

    counters.push_back(system_counter_t("sflow_raw_packet_headers_total", sflow_raw_packet_headers_total,
                                        metric_type_t::counter, sflow_raw_packet_headers_total_desc));
    counters.push_back(system_counter_t("sflow_ipv4_header_protocol", sflow_ipv4_header_protocol,
                                        metric_type_t::counter, sflow_ipv4_header_protocol_desc));
    counters.push_back(system_counter_t("sflow_ipv6_header_protocol", sflow_ipv6_header_protocol,
                                        metric_type_t::counter, sflow_ipv6_header_protocol_desc));
    counters.push_back(system_counter_t("sflow_unknown_header_protocol", sflow_unknown_header_protocol,
                                        metric_type_t::counter, sflow_unknown_header_protocol_desc));
    counters.push_back(system_counter_t("sflow_extended_router_data_records", sflow_extended_router_data_records,
                                        metric_type_t::counter, sflow_extended_router_data_records_desc));
    counters.push_back(system_counter_t("sflow_extended_switch_data_records", sflow_extended_switch_data_records,
                                        metric_type_t::counter, sflow_extended_switch_data_records_desc));
    counters.push_back(system_counter_t("sflow_extended_gateway_data_records", sflow_extended_gateway_data_records,
                                        metric_type_t::counter, sflow_extended_gateway_data_records_desc));

    counters.push_back(system_counter_t("sflow_packets_discarded", sflow_packets_discarded, metric_type_t::counter,
                                        sflow_packets_discarded_desc));


    return counters;
}

// Prototypes

bool process_sflow_counter_sample(const uint8_t* data_pointer,
                                  size_t data_length,
                                  bool expanded,
                                  const sflow_packet_header_unified_accessor& sflow_header_accessor);
process_packet_pointer sflow_process_func_ptr = NULL;

void start_sflow_collector(const std::string& sflow_host, unsigned int sflow_port);

// Initialize sflow module, we need it for allocation per module structures
void init_sflow_module() {
}

// De-initialise sflow module, we need it for deallocation module structures
void deinit_sflow_module() {
}

void start_sflow_collection(process_packet_pointer func_ptr) {
    logger << log4cpp::Priority::INFO << plugin_log_prefix << "plugin started";
    sflow_process_func_ptr = func_ptr;

    if (fastnetmon_global_configuration.sflow_ports.size() == 0) {
        logger << log4cpp::Priority::ERROR << plugin_log_prefix << "Please specify at least single port for sflow_port field";
        return;
    }

    logger << log4cpp::Priority::DEBUG << plugin_log_prefix << "We parsed "
           << fastnetmon_global_configuration.sflow_ports.size() << " ports for sFlow";


    boost::thread_group sflow_collector_threads;

    logger << log4cpp::Priority::DEBUG << plugin_log_prefix << "We will listen on "
           << fastnetmon_global_configuration.sflow_ports.size() << " ports";

    for (auto sflow_port : fastnetmon_global_configuration.sflow_ports) {
        sflow_collector_threads.add_thread(
            new boost::thread(start_sflow_collector, fastnetmon_global_configuration.sflow_host, sflow_port));
    }

    sflow_collector_threads.join_all();
}

void start_sflow_collector(const std::string& sflow_host, unsigned int sflow_port) {

    logger << log4cpp::Priority::INFO << plugin_log_prefix << "plugin will listen on " << sflow_host << ":"
           << sflow_port << " udp port";

    struct addrinfo hints;
    memset(&hints, 0, sizeof hints);

    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;

    // AI_PASSIVE to handle empty sflow_host as bind on all interfaces
    // AI_NUMERICHOST to allow only numerical host
    hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;

    addrinfo* servinfo = NULL;

    std::string port_as_string = std::to_string(sflow_port);

    int getaddrinfo_result = getaddrinfo(sflow_host.c_str(), port_as_string.c_str(), &hints, &servinfo);

    if (getaddrinfo_result != 0) {
        logger << log4cpp::Priority::ERROR << "sFlow getaddrinfo function failed with code: " << getaddrinfo_result
               << " please check sflow_host syntax";
        return;
    }

    int sockfd = socket(servinfo->ai_family, servinfo->ai_socktype, servinfo->ai_protocol);

    if (sockfd == -1) {
        logger << log4cpp::Priority::ERROR << "Cannot create socket with error " << errno << " error message: " << strerror(errno);
        return;
    }

    int bind_result = bind(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);

    if (bind_result != 0) {
        logger << log4cpp::Priority::ERROR << plugin_log_prefix << "cannot bind on " << sflow_port << ":" << sflow_host
               << " with errno: " << errno << " error: " << strerror(errno);
        return;
    }

    freeaddrinfo(servinfo);

    /* We should specify timeout there for correct toolkit shutdown */
    /* Because otherwise recvfrom will stay in blocked mode forever */
    struct timeval tv;
    tv.tv_sec  = 1; /* X Secs Timeout */
    tv.tv_usec = 0; // Not init'ing this can cause strange errors

    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (char*)&tv, sizeof(struct timeval));

    int receive_buffer     = 0;
    socklen_t value_length = sizeof(receive_buffer);

    // Get current read buffer size
   
    // Windows uses char* as 4rd argument: https://learn.microsoft.com/en-gb/windows/win32/api/winsock/nf-winsock-getsockopt and we need to add explicit cast
    // Linux uses void* https://linux.die.net/man/2/setsockopt
    // So I think char* works for both platforms
    int get_buffer_size_res = getsockopt(sockfd, SOL_SOCKET, SO_RCVBUF, (char*)&receive_buffer, &value_length);

    if (get_buffer_size_res != 0) {
        logger << log4cpp::Priority::ERROR << "Cannot retrieve default receive buffer size for sFlow";
    } else {
        logger << log4cpp::Priority::INFO << "Default sFlow receive buffer size: " << receive_buffer << " bytes";
    }

    while (true) {
        unsigned int udp_buffer_size = 65536;
        char udp_buffer[udp_buffer_size];

        struct sockaddr_in client_addr;
        socklen_t address_len = sizeof(client_addr);

        int received_bytes = recvfrom(sockfd, udp_buffer, udp_buffer_size, 0, (struct sockaddr*)&client_addr, &address_len);

        if (received_bytes > 0) {
            raw_udp_packets_received++;

            uint32_t client_ipv4_address = 0;

            if (client_addr.sin_family == AF_INET) {
                client_ipv4_address = client_addr.sin_addr.s_addr;
                // Use most efficient function to avoid bottlenecks here
                std::string ip_address_as_string;
                // convert_ip_as_uint_to_string_fast(client_ipv4_address, ip_address_as_string);
                // logger << log4cpp::Priority::ERROR << "client ip: " << convert_ip_as_uint_to_string(client_ipv4_address);
            } else if (client_addr.sin_family == AF_INET6) {
                // We do not support them now
            } else {
                // Should not happen
            }

            parse_sflow_v5_packet((uint8_t*)udp_buffer, received_bytes, client_ipv4_address);
        } else {
            if (received_bytes == -1) {

                if (errno == EAGAIN) {
                    // We got timeout, it's OK!
                    udp_receive_eagain++;
                } else {
                    udp_receive_errors++;
                    logger << log4cpp::Priority::ERROR << plugin_log_prefix << "data receive failed";
                }
            }
        }

        // Add interruption point for correct application shutdown
        boost::this_thread::interruption_point();
    }
}

bool process_sflow_flow_sample(const uint8_t* data_pointer,
                               size_t data_length,
                               bool expanded,
                               const sflow_packet_header_unified_accessor& sflow_header_accessor,
                               uint32_t client_ipv4_address) {
    const uint8_t* current_packet_end = data_pointer + data_length;

    sflow_sample_header_unified_accessor_t sflow_sample_header_unified_accessor;

    bool read_sflow_sample_header_unified_result =
        read_sflow_sample_header_unified(sflow_sample_header_unified_accessor, data_pointer, data_length, expanded);

    if (!read_sflow_sample_header_unified_result) {
        sflow_bad_flow_samples++;
        logger << log4cpp::Priority::ERROR << plugin_log_prefix << "could not read sample header from the packet";
        return false;
    }

    if (sflow_sample_header_unified_accessor.get_number_of_flow_records() == 0) {
        sflow_bad_flow_samples++;
        logger << log4cpp::Priority::ERROR << plugin_log_prefix << "for some strange reasons we got zero flow records";
        return false;
    }

    const uint8_t* flow_record_zone_start = data_pointer + sflow_sample_header_unified_accessor.get_original_payload_length();

    std::vector<record_tuple_t> vector_tuple;
    vector_tuple.reserve(sflow_sample_header_unified_accessor.get_number_of_flow_records());

    bool padding_found = false;

    bool get_records_result =
        get_records(vector_tuple, flow_record_zone_start,
                    sflow_sample_header_unified_accessor.get_number_of_flow_records(), current_packet_end, padding_found);

    // I think that it's pretty important to have counter for this case
    if (padding_found) {
        sflow_padding_flow_sample++;
    }

    if (!get_records_result) {
        sflow_bad_flow_samples++;
        logger << log4cpp::Priority::ERROR << plugin_log_prefix << "Could not get records for some reasons";
        return false;
    }

    simple_packet_t packet;
    packet.source       = SFLOW;
    packet.arrival_time = current_inaccurate_time;

    packet.agent_ip_address = client_ipv4_address;

    for (auto record : vector_tuple) {
        int32_t record_type        = std::get<0>(record);
        const uint8_t* payload_ptr = std::get<1>(record);
        int32_t record_length      = std::get<2>(record);

        // std::cout << "flow record " << " record_type: " << record_type
        //    << " record_length: " << record_length << std::endl;

        // raw packet header we support only it
        if (record_type == SFLOW_RECORD_TYPE_RAW_PACKET_HEADER) {
            sflow_raw_packet_headers_total++;

            sflow_raw_protocol_header_t sflow_raw_protocol_header;
            memcpy(&sflow_raw_protocol_header, payload_ptr, sizeof(sflow_raw_protocol_header_t));

            sflow_raw_protocol_header.network_to_host_byte_order();
            // logger << log4cpp::Priority::DEBUG << "Raw protocol header: " << sflow_raw_protocol_header.print();

            const uint8_t* header_payload_pointer = payload_ptr + sizeof(sflow_raw_protocol_header_t);

            if (sflow_raw_protocol_header.header_protocol == SFLOW_HEADER_PROTOCOL_ETHERNET) {

                auto result =
                    parse_raw_packet_to_simple_packet_full_ng(header_payload_pointer, sflow_raw_protocol_header.frame_length_before_sampling,
                                                              sflow_raw_protocol_header.header_size, packet,
                                                              true,
                                                              fastnetmon_global_configuration.sflow_read_packet_length_from_ip_header);

                if (result != network_data_stuctures::parser_code_t::success) {
                    sflow_parse_error_nested_header++;

                    logger << log4cpp::Priority::DEBUG << plugin_log_prefix
                           << "Cannot parse nested packet using ng parser: " << parser_code_to_string(result);

                    return false;
                }
            } else if (sflow_raw_protocol_header.header_protocol == SFLOW_HEADER_PROTOCOL_IPv4) {
                // It's IPv4 without Ethernet header at all
                sflow_ipv4_header_protocol++;

                // We parse this packet using special version of our parser which looks only on IPv4 packet
                auto result =
                    parse_raw_ipv4_packet_to_simple_packet_full_ng(header_payload_pointer,
                                                                   sflow_raw_protocol_header.frame_length_before_sampling,
                                                                   sflow_raw_protocol_header.header_size, packet,
                                                                   fastnetmon_global_configuration.sflow_read_packet_length_from_ip_header);

                if (result != network_data_stuctures::parser_code_t::success) {
                    sflow_parse_error_nested_header++;

                    logger << log4cpp::Priority::DEBUG << plugin_log_prefix
                           << "Cannot parse nested IPv4 packet using ng parser: " << parser_code_to_string(result);

                    return false;
                }

            } else if (sflow_raw_protocol_header.header_protocol == SFLOW_HEADER_PROTOCOL_IPv6) {
                // It's IPv6 without Ethernet header at all
                sflow_ipv6_header_protocol++;

                return false;
            } else {
                // Something really unusual, MPLS?
                sflow_unknown_header_protocol++;

                return false;
            }

            // That's actually WEIRD as we touch these fields in packet parser logic too.
            // So basically we do not use logic from parsers above and override values after parser finishes work
	    
            // Pass pointer to raw header to FastNetMon processing functions
            packet.payload_pointer         = header_payload_pointer;
            packet.payload_full_length     = sflow_raw_protocol_header.frame_length_before_sampling;
            packet.captured_payload_length = sflow_raw_protocol_header.header_size;

            packet.sample_ratio = sflow_sample_header_unified_accessor.sampling_rate;

            packet.input_interface  = sflow_sample_header_unified_accessor.input_port_index;
            packet.output_interface = sflow_sample_header_unified_accessor.output_port_index;

            // In sFlow we have special way to learn that packet was discarded from type of output interface
            if (sflow_sample_header_unified_accessor.output_port_type == SFLOW_PORT_FORMAT_PACKET_DISCARDED) {
                packet.forwarding_status = forwarding_status_t::dropped;
                sflow_packets_discarded++;
            }

            // std::cout << print_simple_packet(packet) << std::endl;
        } else if (record_type == SFLOW_RECORD_TYPE_EXTENDED_ROUTER_DATA) {
            sflow_extended_router_data_records++;
        } else if (record_type == SFLOW_RECORD_TYPE_EXTENDED_SWITCH_DATA) {
            sflow_extended_switch_data_records++;
        } else if (record_type == SFLOW_RECORD_TYPE_EXTENDED_GATEWAY_DATA) {
            sflow_extended_gateway_data_records++;

            if (record_length < sizeof(uint32_t)) {
                logger << log4cpp::Priority::ERROR << "Extended gateway data is too short: " << record_length;

                return false;
            }

            // First field here is address type for nexthop
            uint32_t nexthop_address_type = 0;

            memcpy(&nexthop_address_type, payload_ptr, sizeof(uint32_t));

            if (fast_ntoh(nexthop_address_type) == SFLOW_ADDRESS_TYPE_IPv4) {
                // We can parse first more important for us fields from gateway structure
                if (record_length < sizeof(sflow_extended_gateway_information_t)) {
                    logger << log4cpp::Priority::ERROR << "Extended gateway data is too short for IPv structure: " << record_length;
                    return false;
                }

                // We're ready to parse it
                const sflow_extended_gateway_information_t* gateway_details = (const sflow_extended_gateway_information_t*)payload_ptr;

                packet.src_asn = fast_ntoh(gateway_details->router_asn);
                packet.dst_asn = fast_ntoh(gateway_details->source_asn);
            }

            // logger << log4cpp::Priority::DEBUG << "Address type: " << fast_ntoh(*address_type);
        } else {
            // unknown type
        }
    }

    sflow_process_func_ptr(packet);

    return true;
}

// Read sFLOW packet header
// Awesome description about v5 format from AMX-IX folks:
// Header structure from AMS-IX folks:
// http://www.sflow.org/developers/diagrams/sFlowV5Datagram.pdf
void parse_sflow_v5_packet(const uint8_t* payload_ptr, unsigned int payload_length, uint32_t client_ipv4_address) {
    sflow_packet_header_unified_accessor sflow_header_accessor;
    const uint8_t* total_packet_end = payload_ptr + payload_length;

    // Increase total number of packets
    sflow_total_packets++;

    bool read_sflow_header_result = read_sflow_header(payload_ptr, payload_length, sflow_header_accessor);

    if (!read_sflow_header_result) {
        logger << log4cpp::Priority::ERROR << plugin_log_prefix << "could not read sFlow packet header correctly";
        sflow_bad_packets++;
        return;
    }

    if (sflow_header_accessor.get_datagram_samples_count() <= 0) {
        logger << log4cpp::Priority::ERROR << plugin_log_prefix
               << "Strange number of sFlow samples: " << sflow_header_accessor.get_datagram_samples_count();
        sflow_bad_packets++;
        return;
    }

    std::vector<sample_tuple_t> samples_vector;
    samples_vector.reserve(sflow_header_accessor.get_datagram_samples_count());

    const uint8_t* samples_block_start = payload_ptr + sflow_header_accessor.get_original_payload_length();

    bool discovered_padding = false;

    bool get_all_samples_result = get_all_samples(samples_vector, samples_block_start, total_packet_end,
                                                  sflow_header_accessor.get_datagram_samples_count(), discovered_padding);

    if (!get_all_samples_result) {
        logger << log4cpp::Priority::ERROR << plugin_log_prefix << "we could not extract all samples from packet";
        sflow_bad_packets++;
        return;
    }

    if (discovered_padding) {
        sflow_with_padding_at_the_end_of_packet++;
    }

    for (auto sample : samples_vector) {
        // enterprise, sample_format, data start address, data region length
        // std::cout << "We process #" << i << " sample with format " << format
        //    << " enterprise " << enterprise
        //    << " and length " << sample_length << std::endl;

        int32_t enterprise          = std::get<0>(sample);
        int32_t integer_format      = std::get<1>(sample);
        const uint8_t* data_pointer = std::get<2>(sample);
        size_t data_length          = std::get<3>(sample);

        if (enterprise != 0) {
            // We do not support vendor specific additions
            continue;
        }

        sflow_sample_type_t sample_format = sflow_sample_type_from_integer(integer_format);

        if (sample_format == sflow_sample_type_t::BROKEN_TYPE) {
            logger << log4cpp::Priority::ERROR << plugin_log_prefix << "we got broken format type number: " << integer_format;
            continue;
        }

        if (sample_format == sflow_sample_type_t::FLOW_SAMPLE) {
            // logger << log4cpp::Priority::DEBUG << plugin_log_prefix << "We got flow sample";
            process_sflow_flow_sample(data_pointer, data_length, false, sflow_header_accessor, client_ipv4_address);
            sflow_flow_samples++;
        } else if (sample_format == sflow_sample_type_t::COUNTER_SAMPLE) {
            // logger << log4cpp::Priority::DEBUG << plugin_log_prefix << "We got counter sample";
            process_sflow_counter_sample(data_pointer, data_length, false, sflow_header_accessor);
            sflow_counter_sample++;
        } else if (sample_format == sflow_sample_type_t::EXPANDED_FLOW_SAMPLE) {
            // logger << log4cpp::Priority::DEBUG << plugin_log_prefix << "We got expanded flow sample";
            process_sflow_flow_sample(data_pointer, data_length, true, sflow_header_accessor, client_ipv4_address);
            sflow_flow_samples++;
        } else if (sample_format == sflow_sample_type_t::EXPANDED_COUNTER_SAMPLE) {
            // logger << log4cpp::Priority::DEBUG << plugin_log_prefix << "We got expanded counter sample";
            // process_sflow_counter_sample(data_pointer, data_length, true, sflow_header_accessor);
            sflow_expanded_counter_sample++;
        } else {
            logger << log4cpp::Priority::ERROR << plugin_log_prefix << "we got broken format type: " << integer_format;
        }
    }
}

bool process_sflow_counter_sample(const uint8_t* data_pointer,
                                  size_t data_length,
                                  bool expanded,
                                  const sflow_packet_header_unified_accessor& sflow_header_accessor) {
    sflow_counter_header_unified_accessor_t sflow_counter_header_unified_accessor;

    bool read_sflow_counter_header_result =
        read_sflow_counter_header(data_pointer, data_length, expanded, sflow_counter_header_unified_accessor);

    if (!read_sflow_counter_header_result) {
        logger << log4cpp::Priority::ERROR << plugin_log_prefix << "could not read sFlow counter header";
        return false;
    }

    if (sflow_counter_header_unified_accessor.get_number_of_counter_records() == 0) {
        logger << log4cpp::Priority::ERROR << plugin_log_prefix << "get zero number of counter records";
        return false;
    }

    std::vector<counter_record_sample_t> counter_record_sample_vector;
    counter_record_sample_vector.reserve(sflow_counter_header_unified_accessor.get_number_of_counter_records());

    bool get_all_counter_records_result =
        get_all_counter_records(counter_record_sample_vector,
                                data_pointer + sflow_counter_header_unified_accessor.get_original_payload_length(),
                                data_pointer + data_length,
                                sflow_counter_header_unified_accessor.get_number_of_counter_records());

    if (!get_all_counter_records_result) {
        logger << log4cpp::Priority::ERROR << plugin_log_prefix << "could not get all counter records";
        return false;
    }

    for (const auto& counter_record : counter_record_sample_vector) {
        if (counter_record.enterprise != 0) {
            logger << log4cpp::Priority::ERROR << plugin_log_prefix << "we do not support vendor specific enterprise numbers";
            continue;
        }

        sample_counter_types_t sample_type = sample_counter_types_t::BROKEN_COUNTER;

        if (counter_record.format == 1) {
            sample_type = sample_counter_types_t::GENERIC_INTERFACE_COUNTERS;
        } else if (counter_record.format == 2) {
            sample_type = sample_counter_types_t::ETHERNET_INTERFACE_COUNTERS;
        }

        if (sample_type == sample_counter_types_t::GENERIC_INTERFACE_COUNTERS) {
            // logger << log4cpp::Priority::DEBUG << plugin_log_prefix << "GENERIC_INTERFACE_COUNTERS";

            if (sizeof(generic_sflow_interface_counters_t) != counter_record.length) {
                logger << log4cpp::Priority::ERROR << plugin_log_prefix
                       << "length mismatch for generic interface counter packet: " << counter_record.length;
                continue;
            }

            sflow_generic_interface_counter_sample++;

            const generic_sflow_interface_counters_t* generic_sflow_interface_counters =
                (const generic_sflow_interface_counters_t*)(counter_record.pointer);

            if (logger.getPriority() == log4cpp::Priority::DEBUG) {
                logger << log4cpp::Priority::DEBUG << generic_sflow_interface_counters->print();
            }

            // TODO: we need to get relevant fields in special intermediate structure and then pass them for processing
            // somwhere else I think we need one dedicated thread to implement such metrics pusshes to Clickhouse
        } else if (sample_type == sample_counter_types_t::ETHERNET_INTERFACE_COUNTERS) {
            // These counters provide too detailed information about interface and we do not need it on such level for DDoS detection purposes
        }
    }

    return true;
}

