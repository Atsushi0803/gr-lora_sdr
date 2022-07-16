#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "modulate_impl.h"

namespace gr
{
    namespace lora_sdr
    {

        modulate::sptr
        modulate::make(uint8_t sf, uint32_t samp_rate, uint32_t bw, std::vector<uint16_t> sync_words)
        {
            return gnuradio::get_initial_sptr(new modulate_impl(sf, samp_rate, bw, sync_words));
        }
        /*
     * The private constructor
     */
        modulate_impl::modulate_impl(uint8_t sf, uint32_t samp_rate, uint32_t bw, std::vector<uint16_t> sync_words)
            : gr::block("modulate",
                        gr::io_signature::make(1, 1, sizeof(uint32_t)),
                        gr::io_signature::make(1, 1, sizeof(gr_complex)))
        {
            m_sf = sf;
            m_samp_rate = samp_rate;
            m_bw = bw;
            m_sync_words = sync_words;

            m_number_of_bins = (uint32_t)(1u << m_sf);
            m_os_factor = m_samp_rate / m_bw;
            m_samples_per_symbol = (uint32_t)(m_number_of_bins*m_os_factor);

            m_inter_frame_padding = 20; // add some empty symbols at the end of a frame important for transmission with LimeSDR Mini

            m_downchirp.resize(m_samples_per_symbol);
            m_upchirp.resize(m_samples_per_symbol);

            build_ref_chirps(&m_upchirp[0], &m_downchirp[0], m_sf,m_os_factor);

            //Convert given sync word into the two modulated values in preamble
            if(m_sync_words.size()==1){
                uint16_t tmp = m_sync_words[0];
                m_sync_words.resize(2,0);
                m_sync_words[0] = ((tmp&0xF0)>>4)<<3;
                m_sync_words[1] = (tmp&0x0F)<<3;
            }

            n_up = 8;
            symb_cnt = -1;
            preamb_symb_cnt = 0;
            frame_cnt = 0;
            padd_cnt = m_inter_frame_padding;

            set_tag_propagation_policy(TPP_DONT);
            set_output_multiple(m_samples_per_symbol);
        }

        /*
     * Our virtual destructor.
     */
        modulate_impl::~modulate_impl()
        {
        }

        void
        modulate_impl::forecast(int noutput_items, gr_vector_int &ninput_items_required)
        {
            ninput_items_required[0] = 1;
        }
	
	//honda_adding_zone
		uint64_t get_rand_range(uint64_t min_val, uint64_t max_val){
		//radom value generatar
		static std::mt19937_64 mt64(0);
		//min_val, max_val uniform random number
		std::uniform_int_distribution<uint64_t> get_rand_uni_int(min_val, max_val);

		return get_rand_uni_int(mt64);
        }
	//honda_zone_end
	

        int modulate_impl::general_work(int noutput_items,
                                        gr_vector_int &ninput_items,
                                        gr_vector_const_void_star &input_items,
                                        gr_vector_void_star &output_items)
        {
            const uint32_t *in = (const uint32_t *)input_items[0];
            gr_complex *out = (gr_complex *)output_items[0];
            int nitems_to_process = ninput_items[0];
            int output_offset = 0;
	    
	    //honda_adding_zone
	    const uint32_t send_data = m_data;
	    //uint32_t send_data;
	    //uint32_t Tx_num = m_data;
	    //uint32_t Tx_num = m_Tx_num;
	    uint64_t random_data = get_rand_range(1,15);
	    const uint32_t C = 33;
	    const uint32_t interval = 10; //change_every_experiment
	    const uint32_t data_max = 1;
	    const int Tx1_array[] = {6,1,7,3,5,8,2,4};
	    const int Tx2_array[] = {13,10,9,15,12,16,14,11};
	    //honda_zone_end
	    
            // read tags
            //std::vector<tag_t> tags;

            //get_tags_in_window(tags, 0, 0, ninput_items[0], pmt::string_to_symbol("frame_len"));
            //if (tags.size())
            //{
                //if (tags[0].offset != nitems_read(0))
                    //nitems_to_process = std::min(tags[0].offset - nitems_read(0), (uint64_t)(float)noutput_items / m_samples_per_symbol);
                //else
                //{
                    //if (tags.size() >= 2)
                        //nitems_to_process = std::min(tags[1].offset - tags[0].offset, (uint64_t)(float)noutput_items / m_samples_per_symbol);
                    //if ( padd_cnt == m_inter_frame_padding)
                    //{
                        //m_frame_len = pmt::to_long(tags[0].value);
                        //tags[0].offset = nitems_written(0);

                        //tags[0].value = pmt::from_long(int((m_frame_len + m_inter_frame_padding + n_up + 4.25) * m_samples_per_symbol));

                        //add_item_tag(0, tags[0]);

                        //symb_cnt = -1;
                        //preamb_symb_cnt = 0;
                        //padd_cnt = 0;
                    //}
                //}
            //}

	    if(m_npulses == 1){		//between 1pulse and 2pulse
		//if(m_nsymbols == *in+1){
		//if(m_nsymbols == send_data+1){
		//if(m_nsymbols == k_data+1){
		if(m_nsymbols == 1){
			//build_upchirp(&out[0],0,m_sf);
			//build_upchirp(&out[0],0,m_sf,m_os_factor);
			memcpy(&out[output_offset], &m_upchirp[0], m_samples_per_symbol * sizeof(gr_complex));
			//m_npulses++;
			//m_nsymbols = 1;
			//std::cout << "test" << std::endl;
			k_data = Tx2_array[m_send];
			//k_data =2;			

			std::cout <<"send_data_is:"<< send_data <<"k_data, "<<k_data<< std::endl;
			m_send = (m_send+1)%8;
		}
		else {
			for(int j=0; j<128;j++){
		          out[j] = (0,0);
			}
			//if(m_nsymbols == k_data){
			//	m_npulses++;
			//	m_nsymbols = 0;	
			//}

		}
		if(m_nsymbols == k_data){
				m_npulses++;
				m_nsymbols = 0;	
			}
	    } else if(m_npulses == 2){	//between 2pulse and 3pulse
	    //} if(m_npulses == 2){	//between 2pulse and 3pulse
		//if(m_nsymbols == C-(2*(*in))+1){
		//if(m_nsymbols == C-(2*(send_data))+1){
		//if(m_nsymbols == C-(2*(k_data))+1){
		if(m_nsymbols == 1){
			//build_upchirp(&out[0],0,m_sf);
			build_upchirp(&out[0],0,m_sf,m_os_factor);
			//std::cout<<"2pulse_gen"<<std::endl;
			//m_npulses++;
			//m_nsymbols = 1;
			if(k_data == 16){
				m_npulses++;
				m_nsymbols = 0;
			}
		}
		
		else {
			for(int j=0; j<128;j++){
		          out[j] = (0,0);
			}
			//if(m_nsymbols ==  C-(2*(k_data))){
			//	m_npulses++;
			//	m_nsymbols = 0;			
			//}
		if(m_nsymbols ==C-(2*(k_data))){
				m_npulses++;
				m_nsymbols = 0;	
			}
		}
		
	     } else if(m_npulses == 3){	//between 3pulse and 4pulse
	     //} if(m_npulses == 3){	//between 3pulse and 4pulse
	     //} else {	//between 3pulse and 4pulse
		//if(m_nsymbols == *in+1){
		//if(m_nsymbols == send_data+1){
		//if(m_nsymbols == k_data+1){
		if(m_nsymbols == 1){
			//build_upchirp(&out[0],0,m_sf);
			build_upchirp(&out[0],0,m_sf,m_os_factor);
			//std::cout<<"3pulse_gen"<<std::endl;
			//m_npulses = 1;	// if you don't need interval 
			//m_npulses++;
			//m_nsymbols = 1;
			
			//std::cout << "data_num_is:"<<data_cnt << std::endl;
			//std::cout <<""<< std::endl;
		}
		else {
			for(int j=0; j<128;j++){
		          out[j] = (0,0);
			}
			//if(m_nsymbols == k_data){
			//	m_npulses++;
			//	m_nsymbols = 0;	
			//}
		}
		if(m_nsymbols == k_data){
				m_npulses++;
				m_nsymbols = 0;	
			}
	     } else{				// interval and pulse1
	     //} if(m_npulses == 4){
		//if(m_nsymbols == interval+1){	
		if(m_nsymbols == 1){
			data_cnt++;

			//if(data_max <= data_cnt){
			//	std::cout<< "experiment_is_done!!"<<std::endl;
			//	exit(0);
			//}
			//build_upchirp(&out[0],0,m_sf);
			build_upchirp(&out[0],0,m_sf,m_os_factor);
			//m_npulses = 1;
			//m_nsymbols = 0;

			//random_data = get_rand_range(1,10);
			//std::cout << "random:"<< random_data << std::endl;
			
			
			//k_data = Tx1_array[m_send];
			
			//std::cout <<"Tx_num:"<< Tx_num<<"send_data_is:"<< send_data <<"k_data, "<<k_data<< std::endl;
			//m_send = (m_send+1)%8;

			


		}
		else{
			for(int j=0; j<128;j++){
		        	out[j] = (0,0);
			}
			//if(m_nsymbols == interval){
			//	m_npulses = 1;
			//	m_nsymbols = 0;	
			//}	
		}
		if(m_nsymbols == interval){
				m_npulses = 1;
				m_nsymbols = 0;	
		}
	     }
	
	     std::cout << ",m_npulse_is " << m_npulses << " m_symbols_is_" << m_nsymbols << std::endl;
	     //std::cout << "send_data_is"<< *in << std::endl;
	     //std::cout <<"Tx_num, "<< Tx_num<< "send_data_is	"<< k_data << std::endl;

             symb_cnt++;
             m_nsymbols++;
            
                
             
             
              
               
            // if (nitems_to_process)
            //     std::cout << ninput_items[0] << " " << nitems_to_process << " " << output_offset << " " << noutput_items << std::endl;
            consume_each(nitems_to_process);
	    std::cout<<output_offset<<std::endl;
            return output_offset;
        }

    } /* namespace lora */
} /* namespace gr */
