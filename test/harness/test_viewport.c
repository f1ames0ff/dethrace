#include "framework/unity.h"
#include "platforms/platform_viewport.h"

static void test_viewport_exact_fit(void) {
    tHarness_viewport vp;
    Harness_CalculateViewport(320, 200, 320, 200, &vp);
    TEST_ASSERT_EQUAL_INT(0, vp.x);
    TEST_ASSERT_EQUAL_INT(0, vp.y);
    TEST_ASSERT_EQUAL_INT(320, vp.width);
    TEST_ASSERT_EQUAL_INT(200, vp.height);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, vp.scale_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, vp.scale_y);
}

static void test_viewport_letterbox(void) {
    tHarness_viewport vp;
    Harness_CalculateViewport(640, 480, 320, 200, &vp);
    TEST_ASSERT_EQUAL_INT(0, vp.x);
    TEST_ASSERT_EQUAL_INT(40, vp.y);
    TEST_ASSERT_EQUAL_INT(640, vp.width);
    TEST_ASSERT_EQUAL_INT(400, vp.height);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, vp.scale_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.0f, vp.scale_y);
}

static void test_viewport_pillarbox(void) {
    tHarness_viewport vp;
    Harness_CalculateViewport(1000, 500, 320, 200, &vp);
    TEST_ASSERT_EQUAL_INT(100, vp.x);
    TEST_ASSERT_EQUAL_INT(0, vp.y);
    TEST_ASSERT_EQUAL_INT(800, vp.width);
    TEST_ASSERT_EQUAL_INT(500, vp.height);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, vp.scale_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 2.5f, vp.scale_y);
}

static void test_viewport_hires_frame(void) {
    tHarness_viewport vp;
    Harness_CalculateViewport(800, 600, 640, 480, &vp);
    TEST_ASSERT_EQUAL_INT(0, vp.x);
    TEST_ASSERT_EQUAL_INT(0, vp.y);
    TEST_ASSERT_EQUAL_INT(800, vp.width);
    TEST_ASSERT_EQUAL_INT(600, vp.height);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.25f, vp.scale_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.25f, vp.scale_y);
}

static void test_viewport_zero_sizes(void) {
    tHarness_viewport vp;
    Harness_CalculateViewport(0, 0, 320, 200, &vp);
    TEST_ASSERT_EQUAL_INT(0, vp.x);
    TEST_ASSERT_EQUAL_INT(0, vp.y);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, vp.scale_x);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, vp.scale_y);
    Harness_CalculateViewport(640, 480, 0, 0, &vp);
    TEST_ASSERT_FLOAT_WITHIN(0.001f, 1.0f, vp.scale_x);
}

void test_viewport_suite(void) {
    UnitySetTestFile(__FILE__);
    RUN_TEST(test_viewport_exact_fit);
    RUN_TEST(test_viewport_letterbox);
    RUN_TEST(test_viewport_pillarbox);
    RUN_TEST(test_viewport_hires_frame);
    RUN_TEST(test_viewport_zero_sizes);
}
