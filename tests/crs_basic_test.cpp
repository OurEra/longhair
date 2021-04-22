#include "../cauchy_256.h"
#include "../SiameseTools.h"

#include <iostream>
#include <vector>
#include <cstdint>
#include <stdio.h>

using namespace std;
static void printdata(const void *data, int bytes) {
    const uint8_t *in = reinterpret_cast<const uint8_t *>( data );

    cout << hex;
    for (int ii = 0; ii < bytes; ++ii) {
        cout << (int)in[ii] << " ";
    }
    cout << dec << endl;
}

const unsigned block_bytes = 8 * 2; // a multiple of 8
const unsigned message_block_count = 4;
const unsigned recovery_block_count = 2;
const unsigned mock_lost_block = 2;

bool mock_wrong_row = false;

void encode_crs_mock_lost(std::vector<Block>& data_after_transmit) {

    siamese::PCGRandom prng;
    prng.Seed(siamese::GetTimeUsec());

    std::vector<uint8_t> message_blocks(block_bytes * message_block_count);
    std::vector<uint8_t> recovery_blocks(block_bytes * recovery_block_count);

    const uint8_t *message_ptrs[message_block_count];
    for (unsigned ii = 0; ii < message_block_count; ++ii) {
        message_ptrs[ii] = &message_blocks[ii * block_bytes];
    }

    // 1 put random value to block space
    for (unsigned ii = 0; ii < block_bytes * message_block_count; ++ii) {
        message_blocks[ii] = (uint8_t)prng.Next();
    }
    for (int ii = 0; ii < message_block_count; ii++) {
        printf("\nEncode message row(%d) data:\n", ii);
        printdata(message_ptrs[ii], block_bytes);
    }

    // 2 provide block data and generate recovery block
    const int encodeResult = cauchy_256_encode(
        message_block_count,
        recovery_block_count,
        message_ptrs,
        &recovery_blocks[0],
        block_bytes);
    if (encodeResult != 0)
    {
        cout << "Encode failed" << endl;
        SIAMESE_DEBUG_BREAK();
        return;
    }

    // 3 mock lost and put into 'data_after_transmit'
    for (int ii = 0; ii < message_block_count - mock_lost_block; ++ii) {
        memcpy(data_after_transmit[ii].data, (uint8_t*)message_ptrs[ii], block_bytes);
        data_after_transmit[ii].row = mock_wrong_row ? (uint8_t) (message_block_count - mock_lost_block - ii - 1) : (uint8_t)ii;
        if (mock_wrong_row) {
            printf("\nMock wrong row %d => %d\n", ii, (message_block_count - mock_lost_block - ii - 1));
        }
    }

    for (int ii = 0; ii < mock_lost_block; ++ii) {
        memcpy(data_after_transmit[ii + (message_block_count - mock_lost_block)].data, (uint8_t*)&recovery_blocks[ii * block_bytes], block_bytes);
        data_after_transmit[ii + (message_block_count - mock_lost_block)].row = (uint8_t)(ii + message_block_count);
    }
}

void decode_crs(std::vector<Block>& data_after_transmit) {

    for (int ii = 0; ii < message_block_count; ii++) {
        printf("\nBefore decode message row(%d) data:\n", data_after_transmit[ii].row);
        printdata(data_after_transmit[ii].data, block_bytes);
    }

    // decode from part block
    const int decodeResult = cauchy_256_decode(
        message_block_count,
        recovery_block_count,
        &data_after_transmit[0],
        block_bytes);
    if (decodeResult != 0)
    {
        cout << "Decode failed" << endl;
        SIAMESE_DEBUG_BREAK();
        return;
    }

    for (int ii = 0; ii < message_block_count; ii++) {
        printf("\nDecode message row(%d) data:\n", data_after_transmit[ii].row);
        printdata(data_after_transmit[ii].data, block_bytes);
    }
}

int main() {
    cauchy_256_init();

    std::vector<Block> data_space(message_block_count);
    for (int ii = 0; ii < data_space.size(); ii++) {
        data_space[ii].data = new uint8_t[block_bytes];
    }

    encode_crs_mock_lost(data_space);
    decode_crs(data_space);

    for (int ii = 0; ii < data_space.size(); ii++) {
        delete[] data_space[ii].data;
    }

    return 0;
}
