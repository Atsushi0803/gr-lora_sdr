#include "test.h"

std::vector<std::string>
    split( std::string &input, char delimiter ) {
    std::istringstream       stream( input );
    std::string              field;
    std::vector<std::string> result;
    while ( getline( stream, field, delimiter ) ) {
        result.push_back( field );
    }
    return result;
}

int
    main() {
    std::vector<uint32_t> m_l_A;    // 符号語定義のための配列A_i[]
    std::string           input_filepath = "/workspaces/gr-apcma/data_ai/Andi-5pulse.csv";
    std::ifstream         ifs( input_filepath );
    std::string           line;

    while ( std::getline( ifs, line ) ) {
        std::vector<std::string> strvec = split( line, ',' );
        for ( int i = 0; i < strvec.size(); i++ ) {
            m_l_A[i] = stoi( strvec.at( i ) );
        }
    }
}