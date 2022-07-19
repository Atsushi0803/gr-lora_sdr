#ifdef HAVE_CONFIG_H
#    include "config.h"
#endif

#include "modulate_impl.h"

namespace gr {
namespace lora_sdr {

modulate::sptr
    modulate::make( uint8_t   sf,
        uint32_t              samp_rate,
        uint32_t              bw,
        std::vector<uint16_t> sync_words ) {
    return gnuradio::get_initial_sptr(
        new modulate_impl( sf, samp_rate, bw, sync_words ) );
}
/*
 * The private constructor
 */
modulate_impl::modulate_impl( uint8_t sf,
    uint32_t                          samp_rate,
    uint32_t                          bw,
    std::vector<uint16_t>             sync_words ):
    gr::block( "modulate",
        gr::io_signature::make( 1, 1, sizeof( uint32_t ) ),
        gr::io_signature::make( 1, 1, sizeof( gr_complex ) ) ) {
    m_sf         = sf;
    m_samp_rate  = samp_rate;
    m_bw         = bw;
    m_sync_words = sync_words;

    m_number_of_bins     = (uint32_t)( 1u << m_sf );
    m_os_factor          = m_samp_rate / m_bw;
    m_samples_per_symbol = (uint32_t)( m_number_of_bins * m_os_factor );

    // \* ////////////////////////// APCMA edited by Atsushi.N ////////////////////////////////////

    m_npulse = 4;    //パルスの数を定義
    m_nbit   = 3;

    // 送信するデータを定義(ループ状)
    start_var = 1;
    end_var   = 32;
    l_send_data.resize( end_var - start_var + 1 );
    for ( int i = 0; i < ( end_var - start_var + 1 ); i++ ) {
        l_send_data[i] = start_var + i;
    }

    if ( m_npulse == 4 ) {
        m_length_c = 2 ^ ( m_nbit + 1 ) + 5;
    }
    m_codeword.resize( m_length_c );
    l_interval.resize( m_npulse - 1 );
    //////////////////////////////////////////////////////////////////////////////////

    // m_inter_frame_padding = 20; // add some empty symbols at the end of a
    // frame important for transmission with LimeSDR Mini
    m_inter_frame_padding = 500;

    m_downchirp.resize( m_samples_per_symbol );
    m_upchirp.resize( m_samples_per_symbol );
    m_zeros.resize( m_samples_per_symbol );
    build_ref_chirps( &m_upchirp[0], &m_downchirp[0], m_sf, m_os_factor );    // m_upchirpにupchirpを、m_downchirpにdownchirpをそれぞれ格納(../include/utilities.hを参照)
    for ( int i = 0; i < m_samples_per_symbol; i++ ) {                        // 0+j0の配列zerosを定義(0スロット用)
        m_zeros[i] = gr_complex( 0.0, 0.0 );
    }

    // Convert given sync word into the two modulated values in preamble
    if ( m_sync_words.size() == 1 ) {
        uint16_t tmp = m_sync_words[0];
        m_sync_words.resize( 2, 0 );
        m_sync_words[0] = ( ( tmp & 0xF0 ) >> 4 ) << 3;
        m_sync_words[1] = ( tmp & 0x0F ) << 3;
    }

    n_up            = 8;
    // symb_cnt = -1;
    symb_cnt        = 0;    // when css-apcma
    preamb_symb_cnt = 0;
    frame_cnt       = 0;
    padd_cnt        = m_inter_frame_padding;

    set_tag_propagation_policy( TPP_DONT );
    set_output_multiple( m_samples_per_symbol );
}

/*
 * Our virtual destructor.
 */
modulate_impl::~modulate_impl() {}

void
    modulate_impl::forecast( int noutput_items, gr_vector_int &ninput_items_required ) {
    ninput_items_required[0] = 1;
}

int
    modulate_impl::general_work( int noutput_items,
        gr_vector_int               &ninput_items,
        gr_vector_const_void_star   &input_items,
        gr_vector_void_star         &output_items ) {
    const uint32_t    *in                = (const uint32_t *)input_items[0];
    gr_complex        *out               = (gr_complex *)output_items[0];
    int                nitems_to_process = ninput_items[0];
    int                output_offset     = 0;
    // read tags
    std::vector<tag_t> tags;

    // my addings


    if ( frame_cnt == 100 ) {
        std::exit( 1 );
    }

    int tag_length = 30;
    // std::vector<gr_comprex> silent(m_samples_per_symbol, (0.0, 0.0));
    // end

    get_tags_in_window( tags, 0, 0, ninput_items[0], pmt::string_to_symbol( "frame_len" ) );

    if ( tags.size() ) {
        // std::cout<<"tags.size"<<tags.size()<<std::endl;
        // std::cout<<"tags[0]"<<tags[0].value<<std::endl;
        if ( tags[0].offset != nitems_read( 0 ) ) {
            nitems_to_process = std::min( tags[0].offset - nitems_read( 0 ), (uint64_t)(float)noutput_items / m_samples_per_symbol );
        } else {
            if ( tags.size() >= 2 ) {
                nitems_to_process = std::min( tags[1].offset - tags[0].offset, (uint64_t)(float)noutput_items / m_samples_per_symbol );
            }
            // if ( padd_cnt == m_inter_frame_padding)
            if ( tag_length >= 30 ) {
                m_frame_len    = pmt::to_long( tags[0].value );
                // std::cout<<"m_frame_len:
                // "<<m_frame_len<<std::endl;
                tags[0].offset = nitems_written( 0 );

                // tags[0].value =
                // pmt::from_long(int((m_frame_len +
                // m_inter_frame_padding + n_up + 4.25) *
                // m_samples_per_symbol)); tags[0].value =
                // pmt::from_long(tag_length*m_samples_per_symbol);
                tags[0].value = pmt::from_long( int( (m_length_c)*m_samples_per_symbol ) );

                // std::cout<<"tags[0].value:
                // "<<tags[0].value<<std::endl;
                add_item_tag( 0, tags[0] );

                // symb_cnt = -1;
                symb_cnt = 0;

                preamb_symb_cnt = 0;
                padd_cnt        = 0;
            }
        }
    }

    // \* ////////////////////////// APCMA edited by Atsushi.N ////////////////////////////////////

    for ( int i = 0; i < m_length_c; i++ ) {
        m_codeword[i] = 0;
    }
    m_send_data  = l_send_data[m_send_index];
    m_send_index = ( m_send_index + 1 ) % ( end_var - start_var + 1 );    //

    // 符号語の設定
    // x/c-2x/cの場合
    l_interval = { m_send_data, m_length_c - 2 * m_send_data - 4, m_send_data };
    for ( int i = 0; i < m_npulse; i++ ) {
        int tmp_index         = std::accumulate( l_interval.begin(), std::next( l_interval.begin(), i ), 0 ) + i;    // パルスがオンになるm_codewordのインデックスを作成
        m_codeword[tmp_index] = 1;
    }
    for ( int i = 0; i < m_length_c; i++ ) {
        if ( m_codeword[i] == 0 ) {
            memcpy( &out[output_offset], &m_zeros[0], m_samples_per_symbol * sizeof( gr_complex ) );
        } else {
            memcpy( &out[output_offset], &m_upchirp[0], m_samples_per_symbol * sizeof( gr_complex ) );
        }
        output_offset += m_samples_per_symbol;
    }
    for ( int i = 0; i < ( noutput_items - output_offset ) / m_samples_per_symbol; i++ ) {
        if ( padd_cnt < m_inter_frame_padding ) {
            for ( int j = 0; j < m_samples_per_symbol; j++ ) {
                out[output_offset + j] = gr_complex( 0.0, 0.0 );
            }
            output_offset += m_samples_per_symbol;
            symb_cnt++;
            padd_cnt++;
        }
    }

    /////////////////////////// \* Edited by Honda-san ///////////////////////////////////
    // // apcma_1st_pulse
    // if ( symb_cnt == 0 ) {
    //     tag_length++;

    //     memcpy( &out[output_offset], &m_upchirp[0], m_samples_per_symbol * sizeof( gr_complex ) );
    //     output_offset += m_samples_per_symbol;
    //     // std::cout
    //     // <<"1st_pulse_output_offset"<<output_offset<<std::endl;
    //     symb_cnt++;
    // }
    // // silent_time_between_pulse1_and_pulse2
    // for ( int i = 0; i < send_data; i++ ) {
    //     for ( int j = 0; j < m_samples_per_symbol; j++ ) {
    //         out[output_offset + j] = gr_complex( 0.0, 0.0 );
    //     }
    //     // std::cout<<"silent_time_between_1_and_2"<<std::endl;
    //     // mencpy(&out[output_offset], &silent[0], m_samples_per_symbol *
    //     // sizeof(gr_complex));
    //     output_offset += m_samples_per_symbol;
    // }

    // // apcma_2nd_pulse
    // if ( symb_cnt == 1 ) {
    //     memcpy( &out[output_offset], &m_upchirp[0], m_samples_per_symbol * sizeof( gr_complex ) );
    //     output_offset += m_samples_per_symbol;
    //     // std::cout
    //     // <<"2nd_pulse_output_offset"<<output_offset<<std::endl;
    //     symb_cnt++;
    // }
    // // silent_time_between_pulse2_and_pulse3
    // for ( int i = 0; i < apcma_code_length - ( 2 * send_data ); i++ ) {
    //     for ( int j = 0; j < m_samples_per_symbol; j++ ) {
    //         out[output_offset + j] = gr_complex( 0.0, 0.0 );
    //     }
    //     // mencpy(&out[output_offset], &silent[0], m_samples_per_symbol *
    //     // sizeof(gr_complex));
    //     output_offset += m_samples_per_symbol;
    // }
    // // apcma_3rd_pulse
    // if ( symb_cnt == 2 ) {
    //     memcpy( &out[output_offset], &m_upchirp[0], m_samples_per_symbol * sizeof( gr_complex ) );
    //     output_offset += m_samples_per_symbol;
    //     // std::cout
    //     // <<"3rd_pulse_output_offset"<<output_offset<<std::endl;
    //     symb_cnt++;
    // }
    // // silent_time_between_pulse3_and_pulse4
    // for ( int i = 0; i < send_data; i++ ) {
    //     for ( int j = 0; j < m_samples_per_symbol; j++ ) {
    //         out[output_offset + j] = gr_complex( 0.0, 0.0 );
    //     }
    //     // mencpy(&out[output_offset], &silent[0], m_samples_per_symbol *
    //     // sizeof(gr_complex));
    //     output_offset += m_samples_per_symbol;
    // }
    // // apcma_4th_pulse
    // if ( symb_cnt == 3 ) {
    //     memcpy( &out[output_offset], &m_upchirp[0], m_samples_per_symbol * sizeof( gr_complex ) );
    //     output_offset += m_samples_per_symbol;
    //     // std::cout
    //     // <<"4th_pulse_output_offset"<<output_offset<<std::endl;
    //     symb_cnt++;
    //     frame_cnt++;
    //     std::cout << "Frame" << frame_cnt << "sent\t"
    //               << "send_value_is:" << send_data << std::endl;
    // }
    // // std::cout<<"symb_cnt:	"<<symb_cnt<<std::endl;
    // // zeros
    // // if (symb_cnt+apcma_code_length >= m_frame_len) //padd frame end with
    // // zeros
    // if ( symb_cnt == 4 ) {    // padd frame end with zeros
    //     for ( int i = 0; i < ( noutput_items - output_offset ) / m_samples_per_symbol; i++ ) {
    //         if ( padd_cnt < m_inter_frame_padding ) {
    //             for ( int j = 0; j < m_samples_per_symbol; j++ ) {
    //                 out[output_offset + j] = gr_complex( 0.0, 0.0 );
    //             }
    //             output_offset += m_samples_per_symbol;
    //             symb_cnt++;
    //             padd_cnt++;
    //         }
    //     }
    //     // std::cout<<"padd_end"<<padd_cnt<<std::endl;
    //     // std::cout<<"symb_end"<<symb_cnt<<std::endl;
    // }
    // if ( symb_cnt == m_inter_frame_padding + 4 ) {
    // symb_cnt++;
    // std::cout <<"symb_cnt:	"<<symb_cnt <<"\n";
    // std::cout << "Frame " << frame_cnt << " sent\n";
    // }
    if ( nitems_to_process )
        // std::cout << ninput_items[0] << " " << nitems_to_process << "
        // " << output_offset << " " << noutput_items << std::endl;
        // std::cout<<"nitems_to_process
        // "<<nitems_to_process<<std::endl;
        consume_each( nitems_to_process );
    // std::cout <<"output_offset"<<output_offset<<std::endl;
    return output_offset;
}

// APCMA_END

// if (symb_cnt == -1) // preamble
//{
// for (int i = 0; i < noutput_items / m_samples_per_symbol; i++)
//{
// if (preamb_symb_cnt < n_up + 5) //should output preamble part
//{
// if (preamb_symb_cnt < n_up)
//{ //upchirps
// memcpy(&out[output_offset], &m_upchirp[0], m_samples_per_symbol *
// sizeof(gr_complex));
//}
// else if (preamb_symb_cnt == n_up) //sync words
// build_upchirp(&out[output_offset], m_sync_words[0], m_sf,m_os_factor);
// else if (preamb_symb_cnt == n_up + 1)
// build_upchirp(&out[output_offset], m_sync_words[1], m_sf,m_os_factor);

// else if (preamb_symb_cnt < n_up + 4) //2.25 downchirps
// memcpy(&out[output_offset], &m_downchirp[0], m_samples_per_symbol *
// sizeof(gr_complex)); else if (preamb_symb_cnt == n_up + 4)
//{
// memcpy(&out[output_offset], &m_downchirp[0], m_samples_per_symbol / 4 *
// sizeof(gr_complex));
////correct offset dur to quarter of downchirp
// output_offset -= 3 * m_samples_per_symbol / 4;
// symb_cnt = 0;

//}
////my adding
////std::cout<<noutput_items<<std::endl;
////std::cout<<" m_frame_len:"<<<<std::endl;
////end
// output_offset += m_samples_per_symbol;
// preamb_symb_cnt++;
//}
//}
//}

////my addings
////std::cout <<"output_offset_payload_start::"<<output_offset<<std::endl;
////end

// if ( symb_cnt < m_frame_len && symb_cnt>-1) //output payload
//{
// nitems_to_process = std::min(nitems_to_process, int((float)(noutput_items -
// output_offset) / m_samples_per_symbol)); nitems_to_process =
// std::min(nitems_to_process, ninput_items[0]); for (int i = 0; i <
// nitems_to_process; i++)
//{
// build_upchirp(&out[output_offset], in[i], m_sf,m_os_factor);
// output_offset += m_samples_per_symbol;
// symb_cnt++;
//}
//}
// else
//{
// nitems_to_process = 0;
//}

// if (symb_cnt >= m_frame_len) //padd frame end with zeros
//{
// for (int i = 0; i < (noutput_items - output_offset) / m_samples_per_symbol;
// i++)
//{
// if (symb_cnt < m_frame_len + m_inter_frame_padding)
//{
// for (int i = 0; i < m_samples_per_symbol; i++)
//{
// out[output_offset + i] = gr_complex(0.0, 0.0);
//}
// output_offset += m_samples_per_symbol;
// symb_cnt++;
// padd_cnt++;
//}
//}
//}

// if ( symb_cnt+apcma_code_length == m_frame_len + m_inter_frame_padding)
// if ( symb_cnt == m_frame_len + m_inter_frame_padding)
//{
// symb_cnt++;
// frame_cnt++;
// std::cout <<"symb_cnt:	"<<symb_cnt <<"\n";
// std::cout << "Frame " << frame_cnt << " sent\n";
//}
//// if (nitems_to_process)
////     std::cout << ninput_items[0] << " " << nitems_to_process << " " <<
/// output_offset << " " << noutput_items << std::endl;
// consume_each(nitems_to_process);
////std::cout<<"m_frame_len_is::"<<m_frame_len<<std::endl;
////addings
// std::cout <<"output_offset"<<output_offset<<std::endl;
////end
// return output_offset;

//}

}    // namespace lora_sdr
} /* namespace gr */
