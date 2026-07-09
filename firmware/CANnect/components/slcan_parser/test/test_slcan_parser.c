#include "unity.h"
#include "slcan_parser.h"
#include <string.h>

/* ── slcan_parse_open ─────────────────────────────────────────── */

TEST_CASE("slcan_parse_open: valid command", "[slcan]")
{
    TEST_ASSERT_EQUAL(SLCAN_OK, slcan_parse_open("O\r"));
}

TEST_CASE("slcan_parse_open: null argument", "[slcan]")
{
    TEST_ASSERT_EQUAL(SLCAN_INVALID_ARG, slcan_parse_open(NULL));
}

TEST_CASE("slcan_parse_open: wrong command", "[slcan]")
{
    TEST_ASSERT_EQUAL(SLCAN_ERROR, slcan_parse_open("C\r"));
}

TEST_CASE("slcan_parse_open: missing CR", "[slcan]")
{
    TEST_ASSERT_EQUAL(SLCAN_ERROR, slcan_parse_open("O"));
}

/* ── slcan_parse_close ────────────────────────────────────────── */

TEST_CASE("slcan_parse_close: valid command", "[slcan]")
{
    TEST_ASSERT_EQUAL(SLCAN_OK, slcan_parse_close("C\r"));
}

TEST_CASE("slcan_parse_close: null argument", "[slcan]")
{
    TEST_ASSERT_EQUAL(SLCAN_INVALID_ARG, slcan_parse_close(NULL));
}

TEST_CASE("slcan_parse_close: wrong command", "[slcan]")
{
    TEST_ASSERT_EQUAL(SLCAN_ERROR, slcan_parse_close("O\r"));
}

TEST_CASE("slcan_parse_close: missing CR", "[slcan]")
{
    TEST_ASSERT_EQUAL(SLCAN_ERROR, slcan_parse_close("C"));
}

/* ── slcan_parse_bitrate ──────────────────────────────────────── */

TEST_CASE("slcan_parse_bitrate: 500kbps", "[slcan]")
{
    twai_timing_basic_config_t timing = {0};
    TEST_ASSERT_EQUAL(SLCAN_OK, slcan_parse_bitrate("S6\r", &timing));
    TEST_ASSERT_EQUAL_UINT32(500000, timing.bitrate);
}

TEST_CASE("slcan_parse_bitrate: 1Mbps", "[slcan]")
{
    twai_timing_basic_config_t timing = {0};
    TEST_ASSERT_EQUAL(SLCAN_OK, slcan_parse_bitrate("S8\r", &timing));
    TEST_ASSERT_EQUAL_UINT32(1000000, timing.bitrate);
}

TEST_CASE("slcan_parse_bitrate: all valid rates", "[slcan]")
{
    twai_timing_basic_config_t timing = {0};
    const uint32_t expected[] = {
        10000, 20000, 50000, 100000, 125000, 250000, 500000, 800000, 1000000
    };

    for (int i = 0; i <= 8; i++) {
        char cmd[4] = {'S', '0' + i, '\r', '\0'};
        TEST_ASSERT_EQUAL(SLCAN_OK, slcan_parse_bitrate(cmd, &timing));
        TEST_ASSERT_EQUAL_UINT32(expected[i], timing.bitrate);
    }
}

TEST_CASE("slcan_parse_bitrate: null arguments", "[slcan]")
{
    twai_timing_basic_config_t timing = {0};
    TEST_ASSERT_EQUAL(SLCAN_INVALID_ARG, slcan_parse_bitrate(NULL, &timing));
    TEST_ASSERT_EQUAL(SLCAN_INVALID_ARG, slcan_parse_bitrate("S6\r", NULL));
}

TEST_CASE("slcan_parse_bitrate: invalid index", "[slcan]")
{
    twai_timing_basic_config_t timing = {0};
    TEST_ASSERT_EQUAL(SLCAN_NOT_SUPPORTED, slcan_parse_bitrate("S9\r", &timing));
}

TEST_CASE("slcan_parse_bitrate: wrong command", "[slcan]")
{
    twai_timing_basic_config_t timing = {0};
    TEST_ASSERT_EQUAL(SLCAN_ERROR, slcan_parse_bitrate("O6\r", &timing));
}

TEST_CASE("slcan_parse_bitrate: missing CR", "[slcan]")
{
    twai_timing_basic_config_t timing = {0};
    TEST_ASSERT_EQUAL(SLCAN_ERROR, slcan_parse_bitrate("S6", &timing));
}

/* ── slcan_deserialize ────────────────────────────────────────── */

TEST_CASE("slcan_deserialize: standard frame", "[slcan]")
{
    twai_frame_t frame = {0};
    uint8_t buf[8] = {0};

    TEST_ASSERT_EQUAL(SLCAN_OK, slcan_deserialize("t1230811223344556677\r", &frame, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT32(0x123, frame.header.id);
    TEST_ASSERT_EQUAL_UINT8(0, frame.header.ide);
    TEST_ASSERT_EQUAL_UINT8(8, frame.header.dlc);
    TEST_ASSERT_EQUAL_UINT8(0x11, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x77, buf[7]);
}

TEST_CASE("slcan_deserialize: extended frame", "[slcan]")
{
    twai_frame_t frame = {0};
    uint8_t buf[8] = {0};

    TEST_ASSERT_EQUAL(SLCAN_OK, slcan_deserialize("T000004B0211223344\r", &frame, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT32(0x4B0, frame.header.id);
    TEST_ASSERT_EQUAL_UINT8(1, frame.header.ide);
    TEST_ASSERT_EQUAL_UINT8(2, frame.header.dlc);
    TEST_ASSERT_EQUAL_UINT8(0x11, buf[0]);
    TEST_ASSERT_EQUAL_UINT8(0x22, buf[1]);
}

TEST_CASE("slcan_deserialize: zero data length", "[slcan]")
{
    twai_frame_t frame = {0};
    uint8_t buf[8] = {0};

    TEST_ASSERT_EQUAL(SLCAN_OK, slcan_deserialize("t1230\r", &frame, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_UINT8(0, frame.header.dlc);
}

TEST_CASE("slcan_deserialize: null arguments", "[slcan]")
{
    twai_frame_t frame = {0};
    uint8_t buf[8] = {0};

    TEST_ASSERT_EQUAL(SLCAN_INVALID_ARG, slcan_deserialize(NULL, &frame, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(SLCAN_INVALID_ARG, slcan_deserialize("t1230\r", NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(SLCAN_INVALID_ARG, slcan_deserialize("t1230\r", &frame, NULL, sizeof(buf)));
}

TEST_CASE("slcan_deserialize: dlc exceeds 8", "[slcan]")
{
    twai_frame_t frame = {0};
    uint8_t buf[8] = {0};

    TEST_ASSERT_EQUAL(SLCAN_ERROR, slcan_deserialize("t1239\r", &frame, buf, sizeof(buf)));
}

TEST_CASE("slcan_deserialize: unsupported command", "[slcan]")
{
    twai_frame_t frame = {0};
    uint8_t buf[8] = {0};

    TEST_ASSERT_EQUAL(SLCAN_NOT_SUPPORTED, slcan_deserialize("X1230\r", &frame, buf, sizeof(buf)));
}

/* ── slcan_serialize ──────────────────────────────────────────── */

TEST_CASE("slcan_serialize: standard frame", "[slcan]")
{
    uint8_t data[] = {0x11, 0x22, 0x33};
    twai_frame_t frame = {
        .header = {.id = 0x123, .ide = 0, .dlc = 3},
        .buffer = data,
        .buffer_len = 3,
    };
    char buf[32] = {0};

    TEST_ASSERT_EQUAL(SLCAN_OK, slcan_serialize(&frame, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("t123" "3" "112233\r", buf);
}

TEST_CASE("slcan_serialize: extended frame", "[slcan]")
{
    uint8_t data[] = {0xAB, 0xCD};
    twai_frame_t frame = {
        .header = {.id = 0x4B0, .ide = 1, .dlc = 2},
        .buffer = data,
        .buffer_len = 2,
    };
    char buf[32] = {0};

    TEST_ASSERT_EQUAL(SLCAN_OK, slcan_serialize(&frame, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("T000004B0" "2" "ABCD\r", buf);
}

TEST_CASE("slcan_serialize: zero data length", "[slcan]")
{
    twai_frame_t frame = {
        .header = {.id = 0x100, .ide = 0, .dlc = 0},
        .buffer = NULL,
        .buffer_len = 0,
    };
    char buf[32] = {0};

    TEST_ASSERT_EQUAL(SLCAN_OK, slcan_serialize(&frame, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL_STRING("t100" "0\r", buf);
}

TEST_CASE("slcan_serialize: null arguments", "[slcan]")
{
    twai_frame_t frame = {0};
    char buf[32] = {0};

    TEST_ASSERT_EQUAL(SLCAN_INVALID_ARG, slcan_serialize(NULL, buf, sizeof(buf)));
    TEST_ASSERT_EQUAL(SLCAN_INVALID_ARG, slcan_serialize(&frame, NULL, sizeof(buf)));
    TEST_ASSERT_EQUAL(SLCAN_INVALID_ARG, slcan_serialize(&frame, buf, 0));
}

TEST_CASE("slcan_serialize: dlc exceeds 8", "[slcan]")
{
    twai_frame_t frame = {
        .header = {.id = 0x100, .ide = 0, .dlc = 9},
    };
    char buf[32] = {0};

    TEST_ASSERT_EQUAL(SLCAN_ERROR, slcan_serialize(&frame, buf, sizeof(buf)));
}