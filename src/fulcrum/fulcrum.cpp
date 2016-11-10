// Copyright Steinwurf ApS 2016.
// Distributed under the "STEINWURF RESEARCH LICENSE 1.0".
// See accompanying file LICENSE.rst or
// http://www.steinwurf.com/licensing

/// @example fulcrum.cpp
///
/// Simple example showing how to encode and decode a block of data using
/// the Fulcrum codec.
/// For a detailed description of the Fulcrum codec see the following paper
/// on arxiv: http://arxiv.org/abs/1404.6620 by Lucani et. al.

//change some things here

#include <cstdint>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <iostream>
#include <set>
#include <vector>
#include <fstream>
#include <kodocpp/kodocpp.hpp>
#include <kodo_fulcrum/fulcrum_codes.hpp>
#include <storage/storage.hpp>

using namespace std;


template<class SuperCoder>
class my_decoder : public SuperCoder
{
public:
	using stage_one_decoder_type = typename SuperCoder::stage_one_decoder_type;
	using stage_two_decoder_type = typename SuperCoder::stage_two_decoder_type;

    /// @return The stage one decoder
    int stage_one_decoder_rank()
    {
        return SuperCoder::stage_one_decoder().rank();
    }

    /// @return The stage two decoder
    int stage_two_decoder_rank()
    {
    	return SuperCoder::stage_two_decoder().rank();
    }

    using factory = kodo_core::pool_factory<my_decoder>;
};


int run(float e1,float e2, uint32_t expansion);
float min(float a, float b);
ofstream myfile;
int main()
{
    srand((uint32_t)time(0));
	int runs=30;
    std::vector<int> packets_sent;
	myfile.open("out.csv");
	myfile<<"ErasureProb,RelayPackets,DestinationPackets,Expansion,\n";

    // Seed the random number generator to produce different data every time

    for(uint32_t exp=1; exp<=2;exp++)
    	for(int err =0; err<=8; err++)
    	{
    		packets_sent.empty();
    		for(int i=0;i<=runs;i++)
    		{
    			packets_sent.push_back(run(0.1,err*0.1,exp));

    		}
//    		int sum = std::accumulate(packets_sent.begin(), packets_sent.end(), 0);
//    		cout<<"The average packets sent:"<<(float)sum/packets_sent.size()<<endl;
//    		cout<<"The expected number of transmitted packets depends on the error links:"<< 16/min(0.9,1-err*0.1)<<endl;
//    		myfile<<err*0.1<<"," << 16/min(0.9,1-err*0.1)-(float)sum/packets_sent.size()<<","<<(float)sum/packets_sent.size()-16/min(0.9,1-err*0.1)<<","<<exp <<"," <<endl;
    	}
    myfile.close();
    cout<<"run completely \n";
    return 0;
}


int run(float e1,float e2, uint32_t expansion)
{
    // Set the number of symbols and the symbol size
    uint32_t symbols = 32;
    uint32_t max_symbol_size = 1600;

    // Create encoder/decoder factories that we will use to build the actual
    // encoder and decoder
    kodocpp::encoder_factory encoder_factory(
        kodocpp::codec::fulcrum,
        kodocpp::field::binary8,
        symbols,
        max_symbol_size);

    // We query the maximum number of expansion symbols for the fulcrum factory
//    std::cout << "Max expansion of the encoder factory: "
//              << encoder_factory.max_expansion() << std::endl;

    kodo_fulcrum::fulcrum_inner_decoder<fifi::binary>::factory recoder_factory(symbols, max_symbol_size);
    my_decoder<kodo_fulcrum::fulcrum_combined_decoder<fifi::binary8>>::factory decoder_factory(symbols, max_symbol_size);

////    kodocpp::decoder_factory decoder_factory(
//         kodocpp::codec::fulcrum,
//         kodocpp::field::binary8,
//         symbols,
//         max_symbol_size);


//     Before building the encoder/decoder, you can change the number of
//     expansion symbols like this:

     encoder_factory.set_expansion(expansion);
     decoder_factory.set_expansion(expansion);

    kodocpp::encoder encoder = encoder_factory.build();
    auto decoder = decoder_factory.build();


    auto recoder = recoder_factory.build(); //test relays

    // Allocate some storage for a "payload" the payload is what we would
    // eventually send over a network
    std::vector<uint8_t> payload(encoder.payload_size());

    std::vector<uint8_t> payload_recoder(recoder->nested()->payload_size());
//    std::vector<uint8_t> payload_decoder(decoder.payload_size());

    // Allocate input and output data buffers
    std::vector<uint8_t> data_in(encoder.block_size());

    std::vector<uint8_t> data_out1(recoder->block_size());

    std::vector<uint8_t> data_out2(decoder->block_size());

    // Fill the input buffer with random data
    std::generate(data_in.begin(), data_in.end(), rand);

    // Set the symbol storage for the encoder and decoder
    encoder.set_const_symbols(data_in.data(), encoder.block_size());

//    cout<<"Payload size:"<<encoder.payload_size()<< endl;
//    cout<<"Block size (symbols*symbol_size):"<<encoder.block_size()<< endl;
    //recoder->nested()->set_mutable_symbols(storage::storage(data_out1));

    //decoder->set_mutable_symbols(data_out2.data(), decoder->block_size());


    // Install a custom trace function for the decoder
    auto callback = [](const std::string& zone, const std::string& data)
    {
        std::set<std::string> filters =
        {
            "decoder_state", "symbol_coefficients_before_read_symbol"
        };

        if (filters.count(zone))
        {
            std::cout << zone << ":" << std::endl;
            std::cout << data << std::endl;
        }
    };
   // decoder.set_trace_callback(callback);
   //decoder.set_trace_stdout();
//    encoder.set_trace_stdout(); //test tr
    //Add decoder2 callback here

    uint32_t lost_link1 = 0;
    uint32_t lost_link2 = 0;
//    uint32_t received_payloads = 0;
    uint32_t source_packets=0;
    uint32_t destination_packets=0;
    uint32_t relay_packets=0;

    while (!recoder->is_complete())
    {
        // The encoder will use a certain amount of bytes of the payload buffer
        uint32_t bytes_used = encoder.write_payload(payload.data());
//        std::cout << "Payload generated by encoder, bytes used = "
//                  << bytes_used << std::endl;
        source_packets++;
        // Simulate a channel with a e1% loss rate
        if (( (float) rand()/RAND_MAX) < e1)
        {
        	lost_link1++;
            continue;
        }

        recoder->read_payload(payload.data());
    }

    while (!decoder->is_complete())
        {
//        std::cout << "Payload processed by recoder, current rank = "
//                          << recoder->rank() << std::endl << std::endl;
//        recoder->write_payload(payload_recoder.data());
       	recoder->nested()->write_payload(payload_recoder.data());
        relay_packets++;

//         Simulate a channel with a e2 loss rate
        if (( (float) rand()/ RAND_MAX) < e2)
        {
            lost_link2++;
            continue;
        }
        destination_packets++;
       	//decoder.read_payload(payload.data());
        decoder->read_payload(payload_recoder.data());
        std::cout << "current rank of stage one dercoder = "
                  << decoder->stage_one_decoder_rank() << std::endl << std::endl;
        std::cout << "current rank of stage two dercoder = "
                          << decoder->stage_two_decoder_rank() << std::endl << std::endl;
    }

//    std::cout << "Lost link1: " << lost_link1 << std::endl;
//    std::cout << "Lost link2: " << lost_link2 << std::endl;
//    std::cout << "Number of sent packets: " << source_packets<<std::endl;
//    std::cout << "Number of packets sent by relay: " << relay_packets <<std::endl;
//    std::cout << "Number of packets sent by destination: " << destination_packets<<std::endl;
//    std::cout << "Innovative packets measured at Destination: " << destination_packets-symbols<<std::endl;
    // Check if we properly decoded the data
    decoder->copy_from_symbols(storage::storage(data_out2));
//    if (data_in == data_out2)
//    {
//        std::cout << "Data decoded correctly" << std::endl;
//    }
//    else
//    {
//        std::cout << "Unexpected failure to decode, "
//                  << "please file a bug report :)" << std::endl;
//    }

    myfile << e2 << "," <<relay_packets-symbols/(1-e2) << "," << source_packets+relay_packets-(symbols/(1-e1)+symbols/(1-e2))  << "," <<expansion <<"," <<endl;

//    std::cout << source_packets << " "
//    		<< lost_link1 << " "
//			<< relay_packets << " "
//			<< lost_link2 << " "
//			<< destination_packets << endl;

    		//<< source_packets-symbols/(1-e2)<<","<<source_packets-(symbols/min(1-e1,1-e2))<<","<<expansion <<"," <<endl;
//    myfile<<e2<<"," << source_packets-relay_packets<<","<<relay_packets-destination_packets<<","<<expansion <<"," <<endl;
    return source_packets;
}
//
float min(float a, float b){
	return a<b ? a:b;
}
